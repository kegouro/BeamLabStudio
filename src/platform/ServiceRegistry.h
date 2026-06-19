#pragma once

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace beamlab::platform {

// ── ServiceScope ───────────────────────────────────────────────────────

class ServiceRegistry;

class ServiceScope {
public:
    explicit ServiceScope(ServiceRegistry& parent);
    ~ServiceScope();

    ServiceScope(const ServiceScope&) = delete;
    ServiceScope& operator=(const ServiceScope&) = delete;

    template<typename T>
    void registerSingleton(std::unique_ptr<T> instance);

    template<typename T>
    void registerFactory(std::function<std::unique_ptr<T>()> factory);

    bool isRegistered(const std::type_index& type) const;
    std::any tryResolve(const std::type_index& type);

private:
    friend class ServiceRegistry;

    struct Entry {
        std::function<std::any()> factory;
        std::any instance;
        bool ownsInstance{false};
    };

    ServiceRegistry& parent_;
    std::unordered_map<std::type_index, Entry> local_;
};

// ── ServiceRegistry ────────────────────────────────────────────────────

class ServiceRegistry {
public:
    ServiceRegistry() = default;
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    // ── Registration ──────────────────────────────────────────────────

    // Register a pre-built singleton instance.
    template<typename T>
    void registerSingleton(std::unique_ptr<T> instance)
    {
        auto erased = std::any(std::shared_ptr<T>(instance.release()));
        entries_[std::type_index(typeid(T))] = Entry{
            nullptr, erased, true, Lifetime::Singleton
        };
    }

    // Register a lazy factory (called on first resolve, cached thereafter).
    template<typename T>
    void registerFactory(std::function<std::unique_ptr<T>()> factory)
    {
        entries_[std::type_index(typeid(T))] = Entry{
            [f = std::move(factory)]() -> std::any {
                return std::any(std::shared_ptr<T>(f().release()));
            },
            std::any{}, false, Lifetime::Singleton
        };
    }

    // Register a transient factory (new instance on every resolve).
    template<typename T>
    void registerTransient(std::function<std::unique_ptr<T>()> factory)
    {
        entries_[std::type_index(typeid(T))] = Entry{
            [f = std::move(factory)]() -> std::any {
                return std::any(std::shared_ptr<T>(f().release()));
            },
            std::any{}, false, Lifetime::Transient
        };
    }

    // Register auto-wired singleton (constructed from registry on first resolve).
    template<typename T, typename Dep, typename... Deps>
    void registerSingleton()
    {
        registerFactory<T>([this]() -> std::unique_ptr<T> {
            return std::make_unique<T>(resolve<Dep>(), resolve<Deps>()...);
        });
    }

    // Register auto-wired singleton with 0 dependencies (default construct).
    template<typename T>
    void registerSingleton()
    {
        registerFactory<T>([]() -> std::unique_ptr<T> {
            return std::make_unique<T>();
        });
    }

    // Register auto-wired transient.
    template<typename T, typename Dep, typename... Deps>
    void registerTransient()
    {
        registerTransient<T>([this]() -> std::unique_ptr<T> {
            return std::make_unique<T>(resolve<Dep>(), resolve<Deps>()...);
        });
    }

    // ── Resolution ────────────────────────────────────────────────────

    template<typename T>
    T& resolve()
    {
        auto idx = std::type_index(typeid(T));

#ifndef NDEBUG
        detectCircularDependency(idx);
        resolutionStack_.push_back(idx);
        auto popGuard = makeScopeExit([&] { resolutionStack_.pop_back(); });
#endif

        // Check local entries first.
        auto it = entries_.find(idx);
        if (it != entries_.end()) {
            return resolveEntry<T>(it->second);
        }

        // Check active scopes (reverse order — last scope wins).
        for (auto it = activeScopes_.rbegin(); it != activeScopes_.rend(); ++it) {
            auto val = (*it)->tryResolve(idx);
            if (val.has_value()) {
                return *std::any_cast<std::shared_ptr<T>>(val);
            }
        }

        throw std::runtime_error(
            std::string("No service registered for: ") + typeid(T).name());
    }

    template<typename T>
    bool isRegistered() const
    {
        return entries_.count(std::type_index(typeid(T))) > 0;
    }

    // ── Scopes ────────────────────────────────────────────────────────

    ServiceScope createScope()
    {
        return ServiceScope(*this);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────

    void clear()
    {
        entries_.clear();
        activeScopes_.clear();
    }

    std::size_t count() const { return entries_.size(); }

private:
    enum class Lifetime { Singleton, Transient };

    struct Entry {
        std::function<std::any()> factory;
        std::any instance;          // Cached singleton instance.
        bool ownsInstance{false};
        Lifetime lifetime{Lifetime::Singleton};
    };

    template<typename T>
    T& resolveEntry(Entry& entry)
    {
        if (entry.lifetime == Lifetime::Singleton) {
            if (!entry.ownsInstance) {
                entry.instance = entry.factory();
                entry.ownsInstance = true;
            }
            return *std::any_cast<std::shared_ptr<T>&>(entry.instance);
        }

        // Transient: new instance every call.
        auto fresh = entry.factory();
        transients_.push_back(std::any_cast<std::shared_ptr<void>>(fresh));
        return *std::any_cast<std::shared_ptr<T>&>(transients_.back());
    }

#ifndef NDEBUG
    void detectCircularDependency(const std::type_index& idx)
    {
        for (const auto& resolved : resolutionStack_) {
            if (resolved == idx) {
                std::string path;
                for (const auto& t : resolutionStack_) {
                    path += t.name();
                    path += " -> ";
                }
                path += idx.name();
                throw std::runtime_error(
                    "Circular dependency detected: " + path);
            }
        }
    }

    static thread_local std::vector<std::type_index> resolutionStack_;
#endif

    // Helper: execute function on scope exit.
    template<typename F>
    static auto makeScopeExit(F f)
    {
        struct ExitGuard {
            F fn;
            ~ExitGuard() { fn(); }
        };
        return ExitGuard{std::move(f)};
    }

    std::unordered_map<std::type_index, Entry> entries_;
    std::vector<std::shared_ptr<void>> transients_;
    std::vector<ServiceScope*> activeScopes_;

    friend class ServiceScope;
};

// ── ServiceScope inline implementations ────────────────────────────────

inline ServiceScope::ServiceScope(ServiceRegistry& parent)
    : parent_(parent)
{
    parent_.activeScopes_.push_back(this);
}

inline ServiceScope::~ServiceScope()
{
    auto& scopes = parent_.activeScopes_;
    for (auto it = scopes.begin(); it != scopes.end(); ++it) {
        if (*it == this) {
            scopes.erase(it);
            break;
        }
    }
}

template<typename T>
void ServiceScope::registerSingleton(std::unique_ptr<T> instance)
{
    auto erased = std::any(std::shared_ptr<T>(instance.release()));
    local_[std::type_index(typeid(T))] = Entry{
        nullptr, erased, true
    };
}

template<typename T>
void ServiceScope::registerFactory(
    std::function<std::unique_ptr<T>()> factory)
{
    local_[std::type_index(typeid(T))] = Entry{
        [f = std::move(factory)]() -> std::any {
            return std::any(std::shared_ptr<T>(f().release()));
        },
        std::any{}, false
    };
}

inline bool ServiceScope::isRegistered(const std::type_index& type) const
{
    return local_.count(type) > 0;
}

inline std::any ServiceScope::tryResolve(const std::type_index& type)
{
    auto it = local_.find(type);
    if (it == local_.end()) return std::any{};

    auto& entry = it->second;
    if (!entry.ownsInstance) {
        entry.instance = entry.factory();
        entry.ownsInstance = true;
    }
    return entry.instance;
}

} // namespace beamlab::platform
