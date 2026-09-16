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
    return "application/octet-stream";
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

template<typename Stream>
void send_file(Stream& stream,
               const http::request<http::string_body>& req,
               const HttpResponse& response) {
    const auto& fp = response.file_path;
    const auto ct = fp.extension().empty() ? response.content_type : mime_type(fp);

    beast::error_code ec;
    http::file_body::value_type file;
    file.open(fp.string().c_str(), beast::file_mode::scan, ec);
    if (ec) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"File not found"})";
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        http::write(stream, res);
        return;
    }

    http::response<http::file_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, ct);
    res.set(http::field::cache_control, "private, max-age=86400, must-revalidate");
    {
        std::error_code mtime_ec;
        const auto mtime = std::filesystem::last_write_time(fp, mtime_ec);
        if (!mtime_ec) {
            const auto tag = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch())
                    .count());
            res.set(http::field::etag, "\"" + tag + "\"");
        }
    }
    res.body() = std::move(file);
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    http::write(stream, res);
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
            http::write(stream, res);
            break;
        }

        const auto response = router.dispatch(to_http_request(req));

        if (!response.file_path.empty()) {
            send_file(stream, req, response);
            if (!response.keep_alive) {
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
    stream.expires_after(std::chrono::seconds{30});
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
          router(a, j, cfg),
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
