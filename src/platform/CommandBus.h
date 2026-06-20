#pragma once

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <stdexcept>

namespace beamlab::platform {

// ── Predefined commands ────────────────────────────────────────────────

struct ImportFileCommand {
    std::string path;
    std::string profile;
};

struct RunAnalysisCommand {
    std::string datasetId;
    std::string config; // JSON string
};

struct ExportResultsCommand {
    std::string datasetId;
    std::string format;
    std::string outDir;
};

// ── Command handler interface ──────────────────────────────────────────

template<typename TCmd, typename TRes>
class CommandHandler {
public:
    virtual ~CommandHandler() = default;
    virtual TRes handle(const TCmd& command) = 0;
};

// ── CommandBus ─────────────────────────────────────────────────────────

class CommandBus {
public:
    CommandBus() = default;
    CommandBus(const CommandBus&) = delete;
    CommandBus& operator=(const CommandBus&) = delete;

    template<typename TCmd, typename TRes>
    void registerHandler(std::unique_ptr<CommandHandler<TCmd, TRes>> handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto idx = std::type_index(typeid(TCmd));
        if (handlers_.count(idx) > 0) {
            throw std::runtime_error(
                "Handler already registered for command type");
        }
        // Wrap in shared_ptr so the lambda is copyable (std::function requires it).
        auto shared = std::shared_ptr<CommandHandler<TCmd,TRes>>(std::move(handler));
        handlers_[idx] = [shared](const std::any& cmd) -> std::any {
            return shared->handle(std::any_cast<const TCmd&>(cmd));
        };
    }

    template<typename TCmd, typename TRes>
    TRes dispatch(const TCmd& command)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto idx = std::type_index(typeid(TCmd));
        auto it = handlers_.find(idx);
        if (it == handlers_.end()) {
            throw std::runtime_error(
                "No handler registered for command type");
        }
        auto result = it->second(std::make_any<TCmd>(command));
        return std::any_cast<TRes>(result);
    }

    bool hasHandler(const std::type_index& type) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.count(type) > 0;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    using TypeErasedHandler = std::function<std::any(const std::any&)>;

    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, TypeErasedHandler> handlers_;
};

} // namespace beamlab::platform
