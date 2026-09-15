#include "cli/cli.hpp"

#include "core/config.hpp"
#include "core/version.hpp"
#include "db/database.hpp"
#include "db/migration_runner.hpp"
#include "db/sqlite_asset_repository.hpp"
#include "db/sqlite_job_repository.hpp"
#include "server/http_server.hpp"
#include "storage/directory_scanner.hpp"
#include "worker/hash_job_handler.hpp"
#include "worker/job_dispatcher.hpp"
#include "worker/metadata_job_handler.hpp"
#include "worker/preview_job_handler.hpp"
#include "worker/scan_service.hpp"
#include "worker/video_job_handler.hpp"
#include "worker/worker_pool.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace geoframe::cli {

namespace {

// ── Argument parsing ────────────────────────────────────────────────────────

struct Args {
    std::string command;           // serve | scan | status | ""
    std::string positional;        // scan: source path
    std::string source;
    std::string data_dir;
    std::string host;
    std::uint16_t port = 0;
    int threads = 0;
    bool skip_tls = false;
    bool help = false;
    bool version = false;
};

void print_help() {
    std::cout
        << "GeoFrame " << core::version() << " — self-hosted photo library\n\n"
        << "Usage:\n"
        << "  geoframe serve [options]          Start HTTP server + background workers\n"
        << "  geoframe scan <source> [options]  Index a directory (enqueue jobs)\n"
        << "  geoframe status [options]         Show library statistics\n"
        << "  geoframe --version\n"
        << "  geoframe --help\n\n"
        << "Options:\n"
        << "  --source <path>     Source folder with photos/videos\n"
        << "  --data-dir <path>   GeoFrame data dir (default: ~/.local/share/geoframe)\n"
        << "  --host <addr>       Bind address (default: 0.0.0.0)\n"
        << "  --port <n>          HTTPS port (default: 8443)\n"
        << "  --threads <n>       Worker thread count (default: hardware_concurrency-1)\n"
        << "  --skip-tls          Use plain HTTP (dev only, no TLS)\n";
}

std::filesystem::path default_data_dir() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return std::filesystem::path{xdg} / "geoframe";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return std::filesystem::path{home} / ".local" / "share" / "geoframe";
    }
    return std::filesystem::path{"/var/lib/geoframe"};
}

Args parse_args(const int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};

        if (a == "--version") { args.version = true; continue; }
        if (a == "--help" || a == "-h") { args.help = true; continue; }
        if (a == "--skip-tls") { args.skip_tls = true; continue; }

        auto next = [&]() -> std::string_view {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for " + std::string{a});
            return argv[++i];
        };

        if (a == "--source")   { args.source   = next(); continue; }
        if (a == "--data-dir") { args.data_dir = next(); continue; }
        if (a == "--host")     { args.host     = next(); continue; }
        if (a == "--threads")  { args.threads  = std::stoi(std::string{next()}); continue; }
        if (a == "--port") {
            const auto v = std::stoi(std::string{next()});
            if (v < 1 || v > 65535) throw std::runtime_error("Invalid port: " + std::to_string(v));
            args.port = static_cast<std::uint16_t>(v);
            continue;
        }

        if (a == "serve" || a == "scan" || a == "status") {
            args.command = std::string{a};
            // positional: scan <source>
            if (a == "scan" && i + 1 < argc && argv[i + 1][0] != '-') {
                args.positional = argv[++i];
            }
            continue;
        }

        throw std::runtime_error("Unknown argument: " + std::string{a});
    }
    return args;
}

core::Config build_config(const Args& args) {
    core::Config cfg;
    cfg.source         = args.source.empty() ? std::filesystem::path{} : std::filesystem::path{args.source};
    cfg.data_dir       = args.data_dir.empty() ? default_data_dir() : std::filesystem::path{args.data_dir};
    cfg.host           = args.host.empty() ? "0.0.0.0" : args.host;
    cfg.port           = args.port != 0 ? args.port : 8443;
    cfg.worker_threads = args.threads;
    cfg.skip_tls       = args.skip_tls;
    return cfg;
}

// ── DB helpers ──────────────────────────────────────────────────────────────

db::Database open_and_migrate(const core::Config& config) {
    std::filesystem::create_directories(config.data_dir);
    db::Database database{config.data_dir / "geoframe.db"};
    db::MigrationRunner{database}.migrate();
    return database;
}

// ── Disk space check ────────────────────────────────────────────────────────

void check_disk_space(const std::filesystem::path& path) {
    std::error_code ec;
    const auto space = std::filesystem::space(path, ec);
    if (ec) return;
    constexpr auto kWarnMB = 1024ULL;  // 1 GiB
    if (space.available / (1024ULL * 1024) < kWarnMB) {
        spdlog::warn("Low disk space: only {} MB available on {}",
                     space.available / (1024 * 1024), path.string());
    }
}

// ── Commands ────────────────────────────────────────────────────────────────

int cmd_serve(const Args& args) {
    const auto config = build_config(args);
    auto database = open_and_migrate(config);
    check_disk_space(config.data_dir);

    db::SqliteAssetRepository asset_repo{database};
    db::SqliteJobRepository   job_repo{database};

    // Build cache directories
    const auto thumb_dir   = config.data_dir / "cache" / "thumbnails";
    const auto preview_dir = config.data_dir / "cache" / "previews";
    std::filesystem::create_directories(thumb_dir);
    std::filesystem::create_directories(preview_dir);

    // Worker pipeline handlers
    worker::HashJobHandler hash_handler{asset_repo};
    worker::MetadataJobHandler meta_handler{asset_repo};

    worker::VideoJobHandler video_meta_handler{
        asset_repo, core::JobType::VideoMetadata, {}, 0};
    worker::VideoJobHandler video_thumb_handler{
        asset_repo, core::JobType::Thumbnail, thumb_dir, config.thumbnail_max_px};

    worker::PreviewJobHandler image_thumb_handler{
        asset_repo, core::JobType::Thumbnail, thumb_dir, config.thumbnail_max_px};
    worker::PreviewJobHandler image_preview_handler{
        asset_repo, core::JobType::Preview, preview_dir, config.preview_max_px};

    // Dispatcher selects handler by job_type()
    // Image pipeline:  Hash → Metadata → Thumbnail(img) → Preview
    // Video pipeline:  Hash → VideoMetadata → Thumbnail(vid)
    // Two Thumbnail handlers with the same job_type — dispatcher picks the first match.
    // We need to route by media type here. For MVP: use a single Thumbnail dispatcher
    // that checks media type internally. Solution: register both, dispatcher calls
    // the first that matches job_type — VideoJobHandler is registered second, image first.
    // Actually the dispatcher picks the first matching job_type. Since both image_thumb
    // and video_thumb handle JobType::Thumbnail, we need a unified Thumbnail handler.
    //
    // Simple fix: create a dedicated ThumbnailDispatcher or wrap.
    // For MVP: the asset media_type determines which handler to pick at runtime.
    // We'll use a lambda adapter registered as a single Thumbnail handler.

    // Unified thumbnail handler that dispatches based on media type
    struct ThumbnailRouter : worker::IJobHandler {
        core::IAssetRepository& assets;
        worker::PreviewJobHandler& image_handler;
        worker::VideoJobHandler&   video_handler;

        ThumbnailRouter(core::IAssetRepository& a,
                        worker::PreviewJobHandler& i,
                        worker::VideoJobHandler& v)
            : assets(a), image_handler(i), video_handler(v) {}

        core::JobType job_type() const noexcept override {
            return core::JobType::Thumbnail;
        }
        void execute(const core::Job& job) override {
            const auto asset = assets.find_by_id(job.asset_id);
            if (!asset.has_value()) {
                throw std::out_of_range("Asset not found for thumbnail job");
            }
            if (asset->media_type == core::MediaType::Video) {
                video_handler.execute(job);
            } else {
                image_handler.execute(job);
            }
        }
    } thumbnail_router{asset_repo, image_thumb_handler, video_thumb_handler};

    worker::JobDispatcher dispatcher{
        asset_repo, job_repo,
        {&hash_handler, &meta_handler, &video_meta_handler,
         &thumbnail_router, &image_preview_handler}};

    const int thread_count = config.worker_threads > 0
        ? config.worker_threads
        : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);

    worker::WorkerPool pool{job_repo, dispatcher, static_cast<std::size_t>(thread_count)};
    pool.start();

    spdlog::info("Worker pool started with {} threads", thread_count);

    // If source is configured, trigger an immediate scan
    if (!config.source.empty() && std::filesystem::is_directory(config.source)) {
        spdlog::info("Auto-scanning source: {}", config.source.string());
        storage::DirectoryScanner scanner;
        worker::ScanService scan_svc{scanner, asset_repo, job_repo};
        const auto report = scan_svc.scan(config.source);
        spdlog::info("Scan enqueued: {} new, {} existing, {} unsupported, {} errors",
                     report.assets_created, report.already_indexed,
                     report.unsupported, report.filesystem_errors);
    }

    // Run HTTP server (blocking)
    server::HttpServer http_server{config, asset_repo, job_repo};
    http_server.run();

    pool.stop();
    return 0;
}

int cmd_scan(const Args& args) {
    if (args.positional.empty() && args.source.empty()) {
        std::cerr << "Usage: geoframe scan <source-path> [--data-dir <dir>]\n";
        return 1;
    }
    const auto source = std::filesystem::path{
        args.positional.empty() ? args.source : args.positional};

    const auto config = build_config(args);
    auto database = open_and_migrate(config);
    check_disk_space(config.data_dir);

    db::SqliteAssetRepository asset_repo{database};
    db::SqliteJobRepository   job_repo{database};

    storage::DirectoryScanner scanner;
    worker::ScanService scan_svc{scanner, asset_repo, job_repo};

    spdlog::info("Scanning: {}", source.string());
    const auto report = scan_svc.scan(source);

    std::cout << "Files seen:    " << report.files_seen    << '\n'
              << "Assets added:  " << report.assets_created << '\n'
              << "Already known: " << report.already_indexed << '\n'
              << "Unsupported:   " << report.unsupported    << '\n'
              << "Errors:        " << report.filesystem_errors << '\n';
    return 0;
}

int cmd_status(const Args& args) {
    const auto config = build_config(args);
    if (!std::filesystem::exists(config.data_dir / "geoframe.db")) {
        std::cout << "No GeoFrame database found in " << config.data_dir << '\n'
                  << "Run 'geoframe serve' or 'geoframe scan' first.\n";
        return 0;
    }

    auto database = open_and_migrate(config);
    db::SqliteAssetRepository asset_repo{database};

    const auto active  = asset_repo.count(core::AssetStatus::Active);
    const auto trashed = asset_repo.count(core::AssetStatus::Trashed);

    std::error_code ec;
    const auto space = std::filesystem::space(config.data_dir, ec);

    std::cout << "Data dir:      " << config.data_dir  << '\n'
              << "Assets active: " << active            << '\n'
              << "Assets trashed:" << trashed           << '\n';
    if (!ec) {
        std::cout << "Disk free:     " << (space.available / (1024 * 1024)) << " MB\n";
        constexpr auto kWarnMB = 1024ULL;
        if (space.available / (1024ULL * 1024) < kWarnMB) {
            std::cout << "WARNING: low disk space!\n";
        }
    }
    return 0;
}

}  // namespace

// ── Public entry point ─────────────────────────────────────────────────────

int run(const int argc, char* argv[]) {
    try {
        const auto args = parse_args(argc, argv);

        if (args.version) {
            std::cout << "GeoFrame " << core::version() << '\n';
            return 0;
        }
        if (args.help || args.command.empty()) {
            print_help();
            return 0;
        }
        if (args.command == "serve")  return cmd_serve(args);
        if (args.command == "scan")   return cmd_scan(args);
        if (args.command == "status") return cmd_status(args);

        std::cerr << "Unknown command: " << args.command << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}

}  // namespace geoframe::cli
