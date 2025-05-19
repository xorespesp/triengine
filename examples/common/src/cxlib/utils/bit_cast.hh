#pragma once
#include <cxlib/cxlib_defs.h>

#include <type_traits>
#include <cstring>

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
    template <class _Dst, class _Src>
    static inline auto bit_cast(_Src src) noexcept
        -> std::enable_if_t<
            sizeof(_Dst) == sizeof(_Src) &&
            std::is_trivially_copyable_v<_Src> &&
            std::is_trivially_copyable_v<_Dst>
            , _Dst>
    {
        static_assert(std::is_trivially_constructible_v<_Dst>, 
            "Destination type for bit_cast must be trivially constructible.");

        _Dst dst;
        std::memcpy(std::addressof(dst), std::addressof(src), sizeof(_Dst));
        return dst;
    }

} // namespace
_CXLIB_NAMESPACE_END