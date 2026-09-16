#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace geoframe::core {

/**
 * @brief In-process pub/sub for JSON event strings.
 *
 * Used to fan out WebSocket events from the HTTP server without coupling
 * worker/media modules to Boost.Beast.
 */
class EventBus {
public:
    using Handler = std::function<void(const std::string&)>;

    int subscribe(Handler handler);
    void unsubscribe(int subscription_id);
    void publish(const std::string& message);

private:
    std::mutex mutex_;
    std::unordered_map<int, Handler> subscribers_;
    int next_id_ = 1;
};

}  // namespace geoframe::core
