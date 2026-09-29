#pragma once
#include <type_traits>

// Defines bitwise operators for a scoped flags enum.
// Use it in the enum's namespace so the operators are found by ADL.
#define TRIENGINE_DEFINE_ENUM_FLAG_OPERATORS(_ENUM) \
    constexpr _ENUM operator|(const _ENUM a, const _ENUM b) noexcept { \
        using _U = std::underlying_type_t<_ENUM>; return static_cast<_ENUM>(static_cast<_U>(a) | static_cast<_U>(b)); } \
    constexpr _ENUM operator&(const _ENUM a, const _ENUM b) noexcept { \
        using _U = std::underlying_type_t<_ENUM>; return static_cast<_ENUM>(static_cast<_U>(a) & static_cast<_U>(b)); } \
    constexpr _ENUM operator^(const _ENUM a, const _ENUM b) noexcept { \
        using _U = std::underlying_type_t<_ENUM>; return static_cast<_ENUM>(static_cast<_U>(a) ^ static_cast<_U>(b)); } \
    constexpr _ENUM operator~(const _ENUM a) noexcept { \
        using _U = std::underlying_type_t<_ENUM>; return static_cast<_ENUM>(~static_cast<_U>(a)); } \
    constexpr _ENUM& operator|=(_ENUM& a, const _ENUM b) noexcept { return a = a | b; } \
    constexpr _ENUM& operator&=(_ENUM& a, const _ENUM b) noexcept { return a = a & b; } \
    constexpr _ENUM& operator^=(_ENUM& a, const _ENUM b) noexcept { return a = a ^ b; }

namespace triengine::utility
{
    // true if all bits of `flag` are set in `flags`
    template <typename _Enum>
    constexpr bool has_flag(const _Enum flags, const _Enum flag) noexcept {
        static_assert(std::is_enum_v<_Enum>);
        using _U = std::underlying_type_t<_Enum>;
        return (static_cast<_U>(flags) & static_cast<_U>(flag)) == static_cast<_U>(flag);
    }

} // namespace
