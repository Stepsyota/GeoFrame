#include "server/http_server.hpp"

#include "server/progress_broadcaster.hpp"
#include "server/router.hpp"
#include "server/tls.hpp"

#include <spdlog/spdlog.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace geoframe::server {

namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = net::ip::tcp;

// ── MIME type helper ────────────────────────────────────────────────────────

namespace {

std::string mime_type(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")                   return "image/png";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".webp")                  return "image/webp";
    if (ext == ".heic")                  return "image/heic";
    if (ext == ".mp4")                   return "video/mp4";
    if (ext == ".mov")                   return "video/quicktime";
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".css")                   return "text/css";
    if (ext == ".json")                  return "application/json";
    if (ext == ".svg")                   return "image/svg+xml";
    if (ext == ".mvt" || ext == ".pbf")  return "application/vnd.mapbox-vector-tile";
    if (ext == ".pmtiles")               return "application/vnd.pmtiles";
    return "application/octet-stream";
}

struct ByteRange {
    std::uint64_t first = 0;
    std::uint64_t last = 0;
};

std::optional<ByteRange> parse_range_header(const std::string_view header,
                                            const std::uint64_t file_size) {
    constexpr std::string_view prefix{"bytes="};
    if (!header.starts_with(prefix) || file_size == 0) {
        return std::nullopt;
    }

    const auto spec = header.substr(prefix.size());
    const auto dash = spec.find('-');
    if (dash == std::string_view::npos) {
        return std::nullopt;
    }

    const auto start_str = spec.substr(0, dash);
    const auto end_str = spec.substr(dash + 1);

    std::uint64_t first = 0;
    std::uint64_t last = file_size - 1;
    try {
        if (!start_str.empty()) {
            first = std::stoull(std::string{start_str});
        }
        if (!end_str.empty()) {
            last = std::stoull(std::string{end_str});
        }
    } catch (...) {
        return std::nullopt;
    }

    if (first >= file_size || first > last) {
        return std::nullopt;
    }
    if (last >= file_size) {
        last = file_size - 1;
    }
    return ByteRange{first, last};
}

std::string request_path(const std::string_view target) {
    const auto qpos = target.find('?');
    if (qpos == std::string_view::npos) {
        return std::string{target};
    }
    return std::string{target.substr(0, qpos)};
}

bool is_events_websocket(const http::request<http::string_body>& req) {
    return ws::is_upgrade(req) && request_path(req.target()) == "/ws/events";
}

bool is_client_disconnect(const beast::error_code& ec) {
    return ec == beast::errc::broken_pipe || ec == beast::errc::connection_reset ||
           ec == http::error::end_of_stream;
}

HttpRequest to_http_request(const http::request<http::string_body>& req) {
    const auto target = req.target();
    const std::string target_str{target};
    const auto qpos = target_str.find('?');
    const auto path = (qpos == std::string::npos) ? target_str : target_str.substr(0, qpos);
    const auto query = (qpos == std::string::npos) ? std::string{} : target_str.substr(qpos + 1);

    return HttpRequest{
        .method = std::string{req.method_string()},
        .target = target_str,
        .path = path,
        .query = query,
        .body = req.body(),
        .keep_alive = req.keep_alive(),
    };
}

// ── Shared HTTP session logic (plain TCP or TLS) ────────────────────────────

constexpr std::uint64_t kMaxRangeSliceBytes = 16 * 1024 * 1024;
constexpr std::uint64_t kRangeRequiredBytes = 100 * 1024 * 1024;

template<typename Stream>
void write_string_response(Stream& stream, const http::request<http::string_body>& req,
                           http::response<http::string_body>&& res) {
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    beast::error_code write_ec;
    http::write(stream, res, write_ec);
    if (write_ec && !is_client_disconnect(write_ec)) {
        spdlog::debug("HTTP file write error: {}", write_ec.message());
    }
}

template<typename Stream>
void send_file(Stream& stream,
               const http::request<http::string_body>& req,
               const HttpResponse& response) {
    const auto& fp = response.file_path;
    const auto ct = fp.extension().empty() ? response.content_type : mime_type(fp);

    std::error_code size_ec;
    const auto file_size = std::filesystem::file_size(fp, size_ec);
    if (size_ec) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"File not found"})";
        write_string_response(stream, req, std::move(res));
        return;
    }

    const auto range_header = req[http::field::range];
    const auto byte_range = response.range_requests
                                ? parse_range_header(std::string_view{range_header}, file_size)
                                : std::nullopt;

    if (response.range_requests && !byte_range.has_value() &&
        file_size > kRangeRequiredBytes) {
        http::response<http::string_body> res{http::status::range_not_satisfiable,
                                              req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept_ranges, "bytes");
        res.body() = R"({"error":"Range header required for large files"})";
        write_string_response(stream, req, std::move(res));
        return;
    }

    std::optional<std::string> etag;
    {
        std::error_code mtime_ec;
        const auto mtime = std::filesystem::last_write_time(fp, mtime_ec);
        if (!mtime_ec) {
            etag = "\"" + std::to_string(
                               std::chrono::duration_cast<std::chrono::seconds>(
                                   mtime.time_since_epoch())
                                   .count()) +
                   "\"";
        }
    }

    if (byte_range.has_value()) {
        const auto length = byte_range->last - byte_range->first + 1;
        if (length == 0 || length > kMaxRangeSliceBytes) {
            http::response<http::string_body> err{http::status::range_not_satisfiable,
                                                  req.version()};
            err.set(http::field::content_type, "application/json");
            err.body() = R"({"error":"Invalid range"})";
            write_string_response(stream, req, std::move(err));
            return;
        }

        std::ifstream in{fp, std::ios::binary};
        if (!in) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"File not found"})";
            write_string_response(stream, req, std::move(res));
            return;
        }

        in.seekg(static_cast<std::streamoff>(byte_range->first));
        std::string slice(length, '\0');
        in.read(slice.data(), static_cast<std::streamsize>(length));
        if (static_cast<std::uint64_t>(in.gcount()) != length) {
            http::response<http::string_body> err{http::status::range_not_satisfiable,
                                                  req.version()};
            err.set(http::field::content_type, "application/json");
            err.body() = R"({"error":"Invalid range"})";
            write_string_response(stream, req, std::move(err));
            return;
        }

        http::response<http::string_body> res{http::status::partial_content, req.version()};
        res.set(http::field::content_type, ct);
        res.set(http::field::cache_control, "private, max-age=86400, must-revalidate");
        res.set(http::field::accept_ranges, "bytes");
        if (etag.has_value()) {
            res.set(http::field::etag, *etag);
        }
        res.set(http::field::content_range,
                "bytes " + std::to_string(byte_range->first) + "-" +
                    std::to_string(byte_range->last) + "/" + std::to_string(file_size));
        res.body() = std::move(slice);
        write_string_response(stream, req, std::move(res));
        return;
    }

    beast::error_code ec;
    http::file_body::value_type file;
    file.open(fp.string().c_str(), beast::file_mode::scan, ec);
    if (ec) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"File not found"})";
        write_string_response(stream, req, std::move(res));
        return;
    }

    http::response<http::file_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, ct);
    res.set(http::field::cache_control, "private, max-age=86400, must-revalidate");
    if (response.range_requests) {
        res.set(http::field::accept_ranges, "bytes");
    }
    if (etag.has_value()) {
        res.set(http::field::etag, *etag);
    }
    res.body() = std::move(file);
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    beast::error_code write_ec;
    http::write(stream, res, write_ec);
    if (write_ec && !is_client_disconnect(write_ec)) {
        spdlog::debug("HTTP file write error: {}", write_ec.message());
    }
}

template<typename Stream>
void write_json_response(Stream& stream,
                         const http::request<http::string_body>& req,
                         const HttpResponse& response,
                         beast::error_code& ec) {
    http::response<http::string_body> res{
        static_cast<http::status>(response.status), req.version()};
    res.set(http::field::content_type, response.content_type);
    res.set(http::field::access_control_allow_origin, "*");
    res.body() = response.body;
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    http::write(stream, res, ec);
    if (ec && !is_client_disconnect(ec)) {
        spdlog::debug("HTTP JSON write error: {}", ec.message());
    }
}

template<typename Stream>
void handle_websocket_session(Stream stream, http::request<http::string_body> req,
                            core::EventBus& events, ProgressBroadcaster& broadcaster) {
    ws::stream<Stream> ws_stream{std::move(stream)};
    ws_stream.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
    ws_stream.accept(req);

    std::mutex write_mutex;
    const auto send_text = [&](const std::string& payload) {
        std::lock_guard lock{write_mutex};
        beast::error_code write_ec;
        ws_stream.write(net::buffer(payload), write_ec);
    };

    send_text(broadcaster.snapshot_json());

    const int subscription = events.subscribe([&](const std::string& payload) {
        send_text(payload);
    });

    beast::flat_buffer buffer;
    while (true) {
        beast::error_code read_ec;
        ws_stream.read(buffer, read_ec);
        if (read_ec) {
            break;
        }
        buffer.consume(buffer.size());
    }

    events.unsubscribe(subscription);
    beast::error_code close_ec;
    ws_stream.close(ws::close_code::normal, close_ec);
}

template<typename Stream>
void handle_http_session(Stream stream, const Router& router, core::EventBus& events,
                         ProgressBroadcaster& broadcaster) {
    beast::flat_buffer buffer;

    while (true) {
        http::request<http::string_body> req;
        beast::error_code ec;
        http::read(stream, buffer, req, ec);
        if (ec == http::error::end_of_stream || ec == beast::error::timeout) {
            break;
        }
        if (ec) {
            spdlog::debug("HTTP read error: {}", ec.message());
            break;
        }

        if (is_events_websocket(req)) {
            handle_websocket_session(std::move(stream), std::move(req), events, broadcaster);
            return;
        }

        if (ws::is_upgrade(req)) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"Unknown WebSocket endpoint"})";
            res.keep_alive(false);
            res.prepare_payload();
            beast::error_code write_ec;
            http::write(stream, res, write_ec);
            break;
        }

        const auto response = router.dispatch(to_http_request(req));

        if (!response.file_path.empty()) {
            send_file(stream, req, response);
            if (!req.keep_alive()) {
                break;
            }
            continue;
        }

        write_json_response(stream, req, response, ec);
        if (ec || !req.keep_alive()) {
            break;
        }
    }
}

void handle_plain_session(tcp::socket socket, const Router& router, core::EventBus& events,
                          ProgressBroadcaster& broadcaster) {
    beast::tcp_stream stream{std::move(socket)};
    stream.expires_never();
    handle_http_session(std::move(stream), router, events, broadcaster);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

void handle_tls_session(ssl::stream<tcp::socket> stream, const Router& router,
                        core::EventBus& events, ProgressBroadcaster& broadcaster) {
    beast::error_code ec;
    stream.handshake(ssl::stream_base::server, ec);
    if (ec) {
        spdlog::debug("TLS handshake error: {}", ec.message());
        return;
    }

    handle_http_session(std::move(stream), router, events, broadcaster);
    stream.shutdown(ec);
}

}  // namespace

// ── HttpServer::Impl ────────────────────────────────────────────────────────

struct HttpServer::Impl {
    const core::Config& config;
    core::IAssetRepository& assets;
    core::IJobRepository& jobs;
    core::EventBus& events;
    core::ProgressTracker& progress;
    Router router;
    ProgressBroadcaster broadcaster;

    net::io_context io_ctx{1};
    ssl::context ssl_ctx{ssl::context::tlsv12_server};
    std::atomic<bool> running{false};

    Impl(const core::Config& cfg, core::IAssetRepository& a, core::IJobRepository& j,
         core::EventBus& e, core::ProgressTracker& p)
        : config(cfg),
          assets(a),
          jobs(j),
          events(e),
          progress(p),
          router(a, j, cfg, p),
          broadcaster(e, p, a, j) {}
};

// ── HttpServer ──────────────────────────────────────────────────────────────

HttpServer::HttpServer(const core::Config& config,
                       core::IAssetRepository& assets,
                       core::IJobRepository& jobs,
                       core::EventBus& events,
                       core::ProgressTracker& progress)
    : impl_(std::make_unique<Impl>(config, assets, jobs, events, progress)) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::run() {
    auto& impl = *impl_;
    impl.broadcaster.start();

    if (!impl.config.skip_tls) {
        const auto certs_dir = impl.config.data_dir / "certs";
        const auto cert_path = certs_dir / "server.crt";
        const auto key_path  = certs_dir / "server.key";

        ensure_self_signed_cert(cert_path, key_path);

        impl.ssl_ctx.set_options(ssl::context::default_workarounds |
                                 ssl::context::no_sslv2 |
                                 ssl::context::single_dh_use);
        impl.ssl_ctx.use_certificate_file(cert_path.string(), ssl::context::pem);
        impl.ssl_ctx.use_private_key_file(key_path.string(), ssl::context::pem);
    }

    const auto address = net::ip::make_address(impl.config.host);
    const auto port    = impl.config.port;
    tcp::acceptor acceptor{impl.io_ctx, {address, port}};
    acceptor.set_option(net::socket_base::reuse_address{true});

    impl.running = true;
    spdlog::info("GeoFrame listening on {}://{}:{}",
                 impl.config.skip_tls ? "http" : "https",
                 impl.config.host, port);

    while (impl.running) {
        beast::error_code ec;
        tcp::socket socket{impl.io_ctx};
        acceptor.accept(socket, ec);
        if (ec) {
            if (!impl.running) break;
            spdlog::warn("Accept error: {}", ec.message());
            continue;
        }

        if (impl.config.skip_tls) {
            std::thread([sock = std::move(socket), &impl]() mutable {
                handle_plain_session(std::move(sock), impl.router, impl.events, impl.broadcaster);
            }).detach();
        } else {
            std::thread([sock = std::move(socket), &impl]() mutable {
                ssl::stream<tcp::socket> stream{std::move(sock), impl.ssl_ctx};
                handle_tls_session(std::move(stream), impl.router, impl.events, impl.broadcaster);
            }).detach();
        }
    }
}

void HttpServer::stop() {
    impl_->broadcaster.stop();
    impl_->running = false;
    try {
        net::io_context tmp;
        tcp::socket s{tmp};
        s.connect({net::ip::make_address(impl_->config.host), impl_->config.port});
    } catch (...) {
    }
}

}  // namespace geoframe::server
