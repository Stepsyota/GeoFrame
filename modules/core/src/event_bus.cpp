#include "core/event_bus.hpp"

namespace geoframe::core {

int EventBus::subscribe(Handler handler) {
    std::lock_guard lock{mutex_};
    const int id = next_id_++;
    subscribers_.emplace(id, std::move(handler));
    return id;
}

void EventBus::unsubscribe(const int subscription_id) {
    std::lock_guard lock{mutex_};
    subscribers_.erase(subscription_id);
}

void EventBus::publish(const std::string& message) {
    std::lock_guard lock{mutex_};
    for (const auto& [id, handler] : subscribers_) {
        if (handler) {
            handler(message);
        }
    }
}

}  // namespace geoframe::core
