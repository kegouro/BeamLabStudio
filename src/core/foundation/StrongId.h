#pragma once

#include <cstdint>
#include <functional>

namespace beamlab::core {

template <typename Tag>
class StrongId {
public:
    using ValueType = std::uint64_t;

    constexpr StrongId() = default;
    explicit constexpr StrongId(ValueType value) : m_value(value) {}

    [[nodiscard]] constexpr ValueType value() const
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool isValid() const
    {
        return m_value != 0;
    }

    constexpr auto operator<=>(const StrongId&) const = default;

private:
    ValueType m_value{0};
};

} // namespace beamlab::core

namespace std {

template <typename Tag>
struct hash<beamlab::core::StrongId<Tag>> {
    std::size_t operator()(const beamlab::core::StrongId<Tag>& id) const noexcept
    {
        return std::hash<typename beamlab::core::StrongId<Tag>::ValueType>{}(id.value());
    }
};

} // namespace std
