#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <cstdint>
#include <string>

namespace beamlab::platform {

using SubscriptionId = uint64_t;

struct DatasetId {
    uint64_t id;
    bool operator==(const DatasetId& o) const { return id == o.id; }
};

enum class Severity { Info, Warning, Error, Critical };

struct ImportStarted {
    std::string filePath;
    uint64_t fileSize;
};

struct ImportProgress {
    uint64_t bytesRead;
    uint64_t totalBytes;
    double etaSeconds;
};

struct ImportCompleted {
    DatasetId id;
    uint64_t totalSamples;
};

struct AnalysisStarted {
    DatasetId id;
};

struct AnalysisProgress {
    int percentComplete;
    std::string currentStage;
};

struct AnalysisCompleted {
    DatasetId id;
    std::string outputDir;
};

struct ExportStarted {
    std::string format;
};

struct ExportProgress {
    int fileIndex;
    int totalFiles;
};

struct ExportCompleted {
    std::string outputDir;
};

struct ErrorOccurred {
    Severity severity;
    std::string message;
    std::string source;
};

class EventBus;

class ScopedSubscription {
public:
    ScopedSubscription() : bus_(nullptr), id_(0) {}
    ScopedSubscription(EventBus* bus, SubscriptionId id);
    ScopedSubscription(ScopedSubscription&& other) noexcept;
    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept;
    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;
    ~ScopedSubscription();

    void reset();
    SubscriptionId id() const { return id_; }

private:
    EventBus* bus_;
    SubscriptionId id_;
};

class EventBus {
public:
    template<typename E>
    using Handler = std::function<void(const E&)>;

    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template<typename E>
    SubscriptionId subscribe(Handler<E> handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto id = nextId_++;
        auto& entries = handlers_[std::type_index(typeid(E))];
        entries.push_back({id, [h = std::move(handler)](const void* e) {
            h(*static_cast<const E*>(e));
        }});
        return id;
    }

    template<typename E>
    std::unique_ptr<ScopedSubscription> subscribeScoped(Handler<E> handler)
    {
        auto id = subscribe<E>(std::move(handler));
        return std::make_unique<ScopedSubscription>(this, id);
    }

    template<typename E>
    void publish(const E& event)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) return;
        for (auto& entry : it->second) {
            entry.handler(static_cast<const void*>(&event));
        }
    }

    void unsubscribe(SubscriptionId id);
    std::size_t subscriberCount() const;
    void clear();

private:
    struct Entry {
        SubscriptionId id;
        std::function<void(const void*)> handler;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::vector<Entry>> handlers_;
    SubscriptionId nextId_ = 1;
};

} // namespace beamlab::platform
