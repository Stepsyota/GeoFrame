#include "server/http_server.hpp"

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

// ── WebSocket session ──────────────────────────────────────────────────────

class WsSession {
public:
    explicit WsSession(ssl::stream<tcp::socket> stream)
        : ws_(std::move(stream)) {}

    void run(http::request<http::string_body> upgrade_req) {
        ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
        ws_.accept(upgrade_req);

        beast::flat_buffer buffer;
        // Push a simple heartbeat every 5 s; read client messages (ignore them for MVP)
        ws_.async_accept(upgrade_req, [](beast::error_code){});

        // Blocking loop: send progress ping every 5 s
        while (ws_.is_open()) {
            try {
                std::this_thread::sleep_for(std::chrono::seconds{5});
                if (!ws_.is_open()) break;
                ws_.text(true);
                ws_.write(net::buffer(R"({"type":"ping"})"));
            } catch (const std::exception&) {
                break;
            }
        }
    }

private:
    ws::stream<ssl::stream<tcp::socket>> ws_;
};

// ── HTTP session ───────────────────────────────────────────────────────────

void send_file(ssl::stream<tcp::socket>& stream,
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
    res.set(http::field::cache_control, "max-age=3600");
    res.body() = std::move(file);
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    http::write(stream, res);
}

void handle_session(ssl::stream<tcp::socket> stream, const Router& router) {
    beast::error_code ec;
    stream.handshake(ssl::stream_base::server, ec);
    if (ec) {
        spdlog::debug("TLS handshake error: {}", ec.message());
        return;
    }

    beast::flat_buffer buffer;

    while (true) {
        http::request<http::string_body> req;
        http::read(stream, buffer, req, ec);
        if (ec == http::error::end_of_stream || ec == beast::error::timeout) {
            break;
        }
        if (ec) {
            spdlog::debug("HTTP read error: {}", ec.message());
            break;
        }

        // WebSocket upgrade
        if (ws::is_upgrade(req)) {
            WsSession wss{std::move(stream)};
            wss.run(std::move(req));
            return;
        }

        // Convert to our request type
        const auto target = req.target();
        const std::string target_str{target};
        const auto qpos = target_str.find('?');
        const auto path = (qpos == std::string::npos) ? target_str : target_str.substr(0, qpos);
        const auto query = (qpos == std::string::npos) ? std::string{} : target_str.substr(qpos + 1);

        HttpRequest hr{
            .method = std::string{req.method_string()},
            .target = target_str,
            .path = path,
            .query = query,
            .body = req.body(),
            .keep_alive = req.keep_alive(),
        };

        const auto response = router.dispatch(hr);

        // File response
        if (!response.file_path.empty()) {
            send_file(stream, req, response);
            if (!response.keep_alive) break;
            continue;
        }

        // String response
        http::response<http::string_body> res{
            static_cast<http::status>(response.status), req.version()};
        res.set(http::field::content_type, response.content_type);
        res.set(http::field::access_control_allow_origin, "*");
        res.body() = response.body;
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        http::write(stream, res, ec);
        if (ec || !req.keep_alive()) {
            break;
        }
    }

    stream.shutdown(ec);
}

}  // namespace

// ── HttpServer::Impl ────────────────────────────────────────────────────────

struct HttpServer::Impl {
    const core::Config& config;
    core::IAssetRepository& assets;
    core::IJobRepository& jobs;
    Router router;

    net::io_context io_ctx{1};
    ssl::context ssl_ctx{ssl::context::tlsv12_server};
    std::atomic<bool> running{false};

    Impl(const core::Config& cfg, core::IAssetRepository& a, core::IJobRepository& j)
        : config(cfg), assets(a), jobs(j), router(a, j, cfg) {}
};

// ── HttpServer ──────────────────────────────────────────────────────────────

HttpServer::HttpServer(const core::Config& config,
                       core::IAssetRepository& assets,
                       core::IJobRepository& jobs)
    : impl_(std::make_unique<Impl>(config, assets, jobs)) {}

HttpServer::~HttpServer() = default;

void HttpServer::run() {
    auto& impl = *impl_;

    // TLS setup
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

        std::thread([sock = std::move(socket), &impl]() mutable {
            ssl::stream<tcp::socket> stream{std::move(sock), impl.ssl_ctx};
            handle_session(std::move(stream), impl.router);
        }).detach();
    }
}

void HttpServer::stop() {
    impl_->running = false;
    // Wake the acceptor by connecting to itself
    try {
        net::io_context tmp;
        tcp::socket s{tmp};
        s.connect({net::ip::make_address(impl_->config.host), impl_->config.port});
    } catch (...) {
    }
}

}  // namespace geoframe::server
