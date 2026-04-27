#pragma once

#include "core/foundation/Error.h"

#include <optional>
#include <utility>

namespace beamlab::core {

template <typename T>
class Result {
public:
    static Result success(T value)
    {
        return Result(std::move(value), std::nullopt);
    }

    static Result failure(Error error)
    {
        return Result(std::nullopt, std::move(error));
    }

    [[nodiscard]] bool ok() const
    {
        return m_value.has_value();
    }

    [[nodiscard]] const T& value() const
    {
        return *m_value;
    }

    [[nodiscard]] T& value()
    {
        return *m_value;
    }

    [[nodiscard]] const std::optional<Error>& error() const
    {
        return m_error;
    }

private:
    Result(std::optional<T> value, std::optional<Error> error)
        : m_value(std::move(value)), m_error(std::move(error))
    {
    }

    std::optional<T> m_value{};
    std::optional<Error> m_error{};
};

} // namespace beamlab::core
