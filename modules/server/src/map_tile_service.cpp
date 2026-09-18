#include "server/map_tile_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <openssl/err.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/wait.h>
#endif

namespace geoframe::server {

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

std::uint64_t directory_bytes(const std::filesystem::path& root) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec) && !ec) {
            total += static_cast<std::uint64_t>(entry.file_size(ec));
        }
    }
    return total;
}

std::uint64_t count_files(const std::filesystem::path& root) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec) && !ec) {
            ++total;
        }
    }
    return total;
}

void emit_line(const MapTileService::OutputFn& output, const std::string_view text) {
    if (output) {
        output(text);
        return;
    }
    std::cout << text << std::flush;
}

int run_streaming_command(const std::string& cmd, const MapTileService::OutputFn& output) {
    const std::string wrapped = cmd + " 2>&1";
    FILE* pipe = popen(wrapped.c_str(), "r");
    if (pipe == nullptr) {
        return -1;
    }

    std::array<char, 4096> buffer{};
    while (true) {
        const auto read = std::fread(buffer.data(), 1, buffer.size(), pipe);
        if (read == 0) {
            break;
        }
        emit_line(output, std::string_view{buffer.data(), read});
    }

    const int status = pclose(pipe);
#if defined(__linux__)
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
#endif
    return status == 0 ? 0 : 1;
}

std::string url_decode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex = value.substr(i + 1, 2);
            const int code = std::stoi(std::string{hex}, nullptr, 16);
            out.push_back(static_cast<char>(code));
            i += 2;
            continue;
        }
        if (value[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(value[i]);
    }
    return out;
}

std::filesystem::path resolve_staging_dir(const std::filesystem::path& data_dir) {
    const char* env = std::getenv("GEOFRAME_TMP_DIR");
    if (env != nullptr && env[0] != '\0') {
        return std::filesystem::path{env};
    }
    return data_dir / "tmp";
}

}  // namespace

MapTileService::MapTileService(const std::filesystem::path& data_dir)
    : tmp_dir_{resolve_staging_dir(data_dir)},
      bin_dir_{data_dir / "bin"},
      map_dir_{data_dir / "map"},
      archive_dir_{data_dir / "map" / "tiles"},
      cache_dir_{data_dir / "map" / "cache"},
      font_cache_dir_{data_dir / "map" / "fonts"},
      sprite_cache_dir_{data_dir / "map" / "sprite"} {}

std::filesystem::path MapTileService::staging_dir() const { return tmp_dir_; }

std::filesystem::path MapTileService::region_path() const {
    return map_dir_ / kRegionFileName;
}

std::filesystem::path MapTileService::region_staging_path() const {
    return tmp_dir_ / (std::string{kRegionFileName} + ".part");
}

bool MapTileService::has_region() const {
    std::error_code ec;
    return std::filesystem::is_regular_file(region_path(), ec);
}

std::uint64_t MapTileService::region_bytes() const {
    std::error_code ec;
    if (!has_region()) {
        return 0;
    }
    return static_cast<std::uint64_t>(std::filesystem::file_size(region_path(), ec));
}

std::filesystem::path MapTileService::config_path() const { return map_dir_ / "config.json"; }

void MapTileService::save_region_config(const int local_max_zoom, const bool hybrid) const {
    std::filesystem::create_directories(map_dir_);
    const nlohmann::json body{{"localMaxZoom", local_max_zoom}, {"hybrid", hybrid}};
    std::ofstream out{config_path()};
    out << body.dump(2);
}

int MapTileService::region_max_zoom() const {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(config_path(), ec)) {
        return kDefaultWorldMaxZoom;
    }
    std::ifstream in{config_path()};
    const auto parsed = nlohmann::json::parse(in, nullptr, false);
    if (!parsed.is_object() || !parsed.contains("localMaxZoom")) {
        return kDefaultWorldMaxZoom;
    }
    return parsed["localMaxZoom"].get<int>();
}

int MapTileService::offline_max_zoom() const {
    if (!has_region()) {
        return -1;
    }
    return region_max_zoom();
}

std::filesystem::path MapTileService::tile_path(const int z, const int x, const int y,
                                                 const bool archive) const {
    const auto& root = archive ? archive_dir_ : cache_dir_;
    return root / std::to_string(z) / std::to_string(x) / (std::to_string(y) + ".mvt");
}

bool MapTileService::valid_tile_coords(const int z, const int x, const int y) {
    if (z < 0 || z > kUpstreamMaxZoom) {
        return false;
    }
    const int limit = 1 << z;
    return x >= 0 && y >= 0 && x < limit && y < limit;
}

std::string MapTileService::upstream_tile_host(const int x, const int y) {
    static constexpr std::array hosts{"a", "b", "c", "d"};
    const auto index = static_cast<std::size_t>((x + y) % static_cast<int>(hosts.size()));
    return "tiles-" + std::string{hosts[index]} + ".basemaps.cartocdn.com";
}

bool MapTileService::fetch_https(const std::string& host, const std::string& target_path,
                                 const std::filesystem::path& dest) const {
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver{ioc};
        beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw beast::system_error{
                beast::error_code{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()},
                "SSL_set_tlsext_host_name"};
        }

        const auto results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds{20});
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        http::request<http::empty_body> req{http::verb::get, target_path, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "GeoFrame/0.1");

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.shutdown(ec);

        if (res.result() != http::status::ok) {
            spdlog::debug("Map tile upstream {} returned {}", target_path, static_cast<int>(res.result()));
            return false;
        }

        const auto parent = dest.parent_path();
        std::filesystem::create_directories(parent);
        const auto temp = dest.string() + ".part";
        {
            std::ofstream out{temp, std::ios::binary | std::ios::trunc};
            if (!out) {
                return false;
            }
            out.write(res.body().data(), static_cast<std::streamsize>(res.body().size()));
        }
        std::filesystem::rename(temp, dest);
        return true;
    } catch (const std::exception& ex) {
        spdlog::debug("Map tile fetch failed for {}: {}", target_path, ex.what());
        return false;
    }
}

std::optional<std::filesystem::path> MapTileService::tile(const int z, const int x, const int y) {
    if (!valid_tile_coords(z, x, y)) {
        return std::nullopt;
    }

    if (has_region() && z <= offline_max_zoom()) {
        return std::nullopt;
    }

    const auto path = tile_path(z, x, y, false);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return path;
    }

    const auto host = upstream_tile_host(x, y);
    const auto target = "/vectortiles/carto.streets/v1/" + std::to_string(z) + "/" +
                        std::to_string(x) + "/" + std::to_string(y) + ".mvt";
    if (!fetch_https(host, target, path)) {
        return std::nullopt;
    }
    return path;
}

std::optional<std::filesystem::path> MapTileService::font(const std::string& fontstack,
                                                          const std::string& range) {
    const auto decoded = url_decode(fontstack);
    const auto safe_name = decoded;
    const auto path = font_cache_dir_ / safe_name / (range + ".pbf");
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return path;
    }

    const auto target = "/fonts/" + fontstack + "/" + range + ".pbf";
    if (!fetch_https("tiles.basemaps.cartocdn.com", target, path)) {
        return std::nullopt;
    }
    return path;
}

std::optional<std::filesystem::path> MapTileService::sprite(const std::string& name) {
    if (name != "sprite.json" && name != "sprite.png") {
        return std::nullopt;
    }
    const auto path = sprite_cache_dir_ / name;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return path;
    }

    const auto target = "/gl/voyager-gl-style/" + name;
    if (!fetch_https("tiles.basemaps.cartocdn.com", target, path)) {
        return std::nullopt;
    }
    return path;
}

namespace {

struct ParsedHttpsUrl {
    std::string host;
    std::string path;
};

std::optional<ParsedHttpsUrl> parse_https_url(const std::string& url) {
    constexpr std::string_view prefix{"https://"};
    if (!url.starts_with(prefix)) {
        return std::nullopt;
    }
    const auto rest = url.substr(prefix.size());
    const auto slash = rest.find('/');
    if (slash == std::string::npos) {
        return ParsedHttpsUrl{.host = rest, .path = "/"};
    }
    return ParsedHttpsUrl{.host = rest.substr(0, slash), .path = rest.substr(slash)};
}

}  // namespace

bool MapTileService::https_head_ok(const std::string& host, const std::string& target_path,
                                   std::uint64_t* content_length) {
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver{ioc};
        beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            return false;
        }

        const auto results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds{5});
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        http::request<http::empty_body> req{http::verb::head, target_path, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "GeoFrame/0.1");
        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::empty_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.shutdown(ec);

        if (res.result() != http::status::ok) {
            return false;
        }
        if (content_length != nullptr) {
            const auto header = res[http::field::content_length];
            if (!header.empty()) {
                *content_length = std::stoull(std::string{header});
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string MapTileService::latest_protomaps_planet_url() {
    constexpr const char* kFallback = "https://build.protomaps.com/20241230.pmtiles";

    // Protomaps planet builds are infrequent; quick curl probe (last 7 days), then fall back.
    const auto now = std::chrono::system_clock::now();
    for (int days = 0; days < 7; ++days) {
        const auto day = now - std::chrono::hours{24 * days};
        const std::time_t time = std::chrono::system_clock::to_time_t(day);
        std::tm tm{};
        gmtime_r(&time, &tm);

        char dated[16];
        std::snprintf(dated, sizeof(dated), "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                       tm.tm_mday);
        const std::string url =
            "https://build.protomaps.com/" + std::string{dated} + ".pmtiles";
        const std::string cmd = "curl -sfI -m 3 -o /dev/null \"" + url + "\"";
        if (std::system(cmd.c_str()) == 0) {
            return url;
        }
    }

    return kFallback;
}

std::filesystem::path MapTileService::bundled_pmtiles_path() const {
    return bin_dir_ / "pmtiles";
}

std::optional<std::filesystem::path> MapTileService::find_pmtiles_executable() const {
    if (std::system("command -v pmtiles >/dev/null 2>&1") == 0) {
        return std::filesystem::path{"pmtiles"};
    }

    std::error_code ec;
    const auto bundled = bundled_pmtiles_path();
    if (std::filesystem::is_regular_file(bundled, ec)) {
        return bundled;
    }

    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        const auto local_bin = std::filesystem::path{home} / ".local" / "bin" / "pmtiles";
        if (std::filesystem::is_regular_file(local_bin, ec)) {
            return local_bin;
        }
    }

    return std::nullopt;
}

bool MapTileService::ensure_pmtiles_tool(const OutputFn& output) const {
    if (find_pmtiles_executable().has_value()) {
        return true;
    }

    emit_line(output, "Downloading pmtiles tool (~55 MB)…\n");

    std::filesystem::create_directories(bin_dir_);
    const auto dest = bundled_pmtiles_path();

#if defined(__linux__) && defined(__x86_64__)
    const char* url =
        "https://github.com/protomaps/go-pmtiles/releases/download/v1.31.2/"
        "go-pmtiles_1.31.2_Linux_x86_64.tar.gz";
#elif defined(__linux__) && defined(__aarch64__)
    const char* url =
        "https://github.com/protomaps/go-pmtiles/releases/download/v1.31.2/"
        "go-pmtiles_1.31.2_Linux_arm64.tar.gz";
#else
    spdlog::error("Auto-install for pmtiles is only supported on Linux x86_64/arm64");
    return false;
#endif

    const std::string cmd = "curl -f# \"" + std::string{url} + "\" | tar xz -C \"" +
                            bin_dir_.string() + "\" pmtiles";
    if (run_streaming_command(cmd, output) != 0) {
        return false;
    }
    emit_line(output, "\n");

    std::error_code ec;
    if (!std::filesystem::is_regular_file(dest, ec)) {
        return false;
    }
    std::filesystem::permissions(dest, std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add, ec);
    return !ec;
}

bool MapTileService::extract_world_subset(const std::string& source_url, const int max_zoom,
                                          const OutputFn& output, const bool dry_run_only) {
    if (max_zoom < 0 || max_zoom > 15) {
        return false;
    }

    if (!ensure_pmtiles_tool(output)) {
        spdlog::error("pmtiles CLI not available and auto-install failed");
        return false;
    }

    const auto pmtiles = find_pmtiles_executable();
    if (!pmtiles.has_value()) {
        return false;
    }

    std::filesystem::create_directories(map_dir_);
    std::filesystem::create_directories(tmp_dir_);
    const auto dest = region_path();
    const auto temp = region_staging_path().string();

    const auto base_cmd = "\"" + pmtiles->string() + "\" extract --maxzoom=" +
                          std::to_string(max_zoom) + " --download-threads=8 \"" + source_url +
                          "\" \"" + temp + "\"";

    emit_line(output, "Estimating download size (usually 30–60 s)…\n");
    if (run_streaming_command(base_cmd + " --dry-run", output) != 0) {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
        return false;
    }

    if (dry_run_only) {
        emit_line(output, "\nDry run complete — no file written.\n");
        return true;
    }

    emit_line(output, "\nBuilding offline map file — live progress below:\n");
    std::atomic<bool> extract_running{true};
    const std::filesystem::path temp_path = region_staging_path();
    std::thread monitor([&]() {
        std::uint64_t last_mb = 0;
        while (extract_running) {
            std::this_thread::sleep_for(std::chrono::seconds{2});
            std::error_code ec;
            if (!std::filesystem::exists(temp_path, ec)) {
                continue;
            }
            const auto bytes = std::filesystem::file_size(temp_path, ec);
            if (ec) {
                continue;
            }
            const auto mb = bytes / (1024 * 1024);
            if (mb != last_mb) {
                last_mb = mb;
                emit_line(output, "  → region.pmtiles on disk: " + std::to_string(mb) + " MB\n");
            }
        }
    });

    const int extract_status = run_streaming_command(base_cmd, output);
    extract_running = false;
    if (monitor.joinable()) {
        monitor.join();
    }

    if (extract_status != 0) {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(temp, dest, ec);
    if (!ec) {
        save_region_config(max_zoom, true);
        const auto final_mb = region_bytes() / (1024 * 1024);
        emit_line(output, "Done. Offline map: " + std::to_string(final_mb) + " MB (zoom 0–" +
                                    std::to_string(max_zoom) + ")\n");
    }
    return !ec;
}

bool MapTileService::fetch_https_to_file(
    const std::string& host, const std::string& target_path, const std::filesystem::path& dest,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const {
    try {
        std::filesystem::create_directories(dest.parent_path());
        const auto temp = dest.string() + ".part";

        std::uint64_t resume_from = 0;
        std::error_code size_ec;
        if (std::filesystem::exists(temp, size_ec)) {
            resume_from = std::filesystem::file_size(temp, size_ec);
        }

        std::uint64_t total = 0;
        if (!https_head_ok(host, target_path, &total) && resume_from == 0) {
            return false;
        }

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver{ioc};
        beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            return false;
        }

        const auto results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds{300});
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        http::request<http::empty_body> req{http::verb::get, target_path, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "GeoFrame/0.1");
        if (resume_from > 0) {
            req.set(http::field::range, "bytes=" + std::to_string(resume_from) + "-");
        }
        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response_parser<http::file_body> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());

        {
            http::file_body::value_type body;
            beast::error_code file_ec;
            const auto mode = resume_from > 0 ? beast::file_mode::append : beast::file_mode::write;
            body.open(temp.c_str(), mode, file_ec);
            if (file_ec) {
                return false;
            }
            parser.get().body() = std::move(body);
        }

        http::read_header(stream, buffer, parser);
        const auto status = parser.get().result();
        if (status != http::status::ok && status != http::status::partial_content) {
            return false;
        }

        if (total == 0) {
            const auto content_length = parser.get()[http::field::content_length];
            if (!content_length.empty()) {
                const auto chunk = std::stoull(std::string{content_length});
                total = resume_from + chunk;
            }
        }

        while (!parser.is_done()) {
            http::read_some(stream, buffer, parser);
            if (on_progress) {
                std::error_code progress_ec;
                const auto downloaded = std::filesystem::file_size(temp, progress_ec);
                if (!progress_ec) {
                    on_progress(downloaded, total);
                }
            }
        }

        parser.get().body().close();
        beast::error_code ec;
        stream.shutdown(ec);
        std::filesystem::rename(temp, dest);
        return true;
    } catch (const std::exception& ex) {
        spdlog::debug("Region download failed for {}: {}", target_path, ex.what());
        return false;
    }
}

bool MapTileService::download_region(
    const std::string& url,
    const std::function<void(std::uint64_t downloaded, std::uint64_t total)>& on_progress) {
    const auto parsed = parse_https_url(url);
    if (!parsed.has_value()) {
        return false;
    }

    std::filesystem::create_directories(map_dir_);
    std::filesystem::create_directories(tmp_dir_);
    const auto staging = region_staging_path();
    if (!fetch_https_to_file(parsed->host, parsed->path, staging, on_progress)) {
        return false;
    }
    std::error_code ec;
    std::filesystem::rename(staging, region_path(), ec);
    return !ec;
}

std::uint64_t MapTileService::archive_bytes() const { return directory_bytes(archive_dir_); }

std::uint64_t MapTileService::cache_bytes() const { return directory_bytes(cache_dir_); }

std::uint64_t MapTileService::archive_tile_count() const { return count_files(archive_dir_); }

std::uint64_t MapTileService::cache_tile_count() const { return count_files(cache_dir_); }

}  // namespace geoframe::server
