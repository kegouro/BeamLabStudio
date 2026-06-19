#include "platform/EventBus.h"

namespace beamlab::platform {

// ── ScopedSubscription ──────────────────────────────────────────────────

ScopedSubscription::ScopedSubscription(EventBus* bus, SubscriptionId id)
    : bus_(bus), id_(id)
{
}

ScopedSubscription::ScopedSubscription(ScopedSubscription&& other) noexcept
    : bus_(other.bus_), id_(other.id_)
{
    other.bus_ = nullptr;
    other.id_ = 0;
}

ScopedSubscription& ScopedSubscription::operator=(ScopedSubscription&& other) noexcept
{
    if (this != &other) {
        reset();
        bus_ = other.bus_;
        id_ = other.id_;
        other.bus_ = nullptr;
        other.id_ = 0;
    }
    return *this;
}

ScopedSubscription::~ScopedSubscription()
{
    reset();
}

void ScopedSubscription::reset()
{
    if (bus_ != nullptr && id_ != 0) {
        bus_->unsubscribe(id_);
        bus_ = nullptr;
        id_ = 0;
    }
}

// ── EventBus ────────────────────────────────────────────────────────────

void EventBus::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, entries] : handlers_) {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->id == id) {
                entries.erase(it);
                if (entries.empty()) {
                    // Keep the key in the map to avoid iterator invalidation
                    // for concurrent publish calls — the empty vector check
                    // in publish() handles this.
                }
                return;
            }
        }
    }
}

std::size_t EventBus::subscriberCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& [_, entries] : handlers_) {
        total += entries.size();
    }
    return total;
}

void EventBus::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.clear();
}

} // namespace beamlab::platform
