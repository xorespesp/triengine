#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

// Byte layout of GLSL interface blocks (uniform blocks and shader storage blocks) under std140 and std430.
// Both are standard layouts: the spec fixes every offset, so the values below hold on every implementation,
// as long as the GLSL block is declared `layout(std140)` or `layout(std430)` (uniform blocks default to `shared`).
// Ref: OpenGL 4.6 core profile spec, 7.6.2.2 "Standard Uniform Block Layout"
//      https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
namespace triengine::core::shader_block
{
    enum class layout_rule {
        std140, // uniform blocks; array strides and structs are rounded up to 16 bytes
        std430, // shader storage blocks only; no such rounding
    };

    inline constexpr size_t kVec4Alignment = 16;

    // GLSL types as C++ tags, with their layout written out.
    //   `_Alignment`: the offset of a value must be a multiple of it
    //   `_Size`: bytes a value takes; the next member may start right after (a vec3 aligns to 16 but takes 12)
    //   `_Std140ArrayStride`, `_Std430ArrayStride`: distance between array elements under each rule
    // `_Columns > 1` is a col-major matrix, laid out as an array of its column vectors.
    namespace glsl
    {
        template <
            class _Component, 
            uint32_t _Columns, uint32_t _Rows,
            size_t _Alignment, size_t _Size, 
            size_t _Std140ArrayStride, size_t _Std430ArrayStride
        >
        struct type
        {
            static_assert(std::is_same_v<_Component, float> || std::is_same_v<_Component, int32_t> || std::is_same_v<_Component, uint32_t>,
                "GLSL component must be float, int or uint");
            static_assert(_Columns >= 1 && _Columns <= 4 && _Rows >= 1 && _Rows <= 4, "GLSL types have 1 to 4 columns and rows");
            static_assert(_Columns == 1 || (_Rows >= 2 && std::is_same_v<_Component, float>), "GLSL matrices are float with 2 to 4 rows");

            // Catch typos in the table below.
            static_assert(_Size >= sizeof(_Component) * _Columns * _Rows, "smaller than its components");
            static_assert(_Std430ArrayStride >= _Size && _Std430ArrayStride % _Alignment == 0, "array elements would overlap or misalign");
            static_assert(_Std140ArrayStride >= _Std430ArrayStride && _Std140ArrayStride % kVec4Alignment == 0, "std140 strides are multiples of 16");

            using component_type = _Component;
            static constexpr uint32_t columns = _Columns;
            static constexpr uint32_t rows = _Rows;
            static constexpr size_t alignment = _Alignment;
            static constexpr size_t size = _Size;

            // Distance between consecutive columns of a matrix; 0 for a non-matrix.
            static constexpr size_t column_stride = _Columns == 1 ? 0 : _Size / _Columns;

            static constexpr size_t array_stride(const layout_rule rule) noexcept {
                return rule == layout_rule::std140 ? _Std140ArrayStride : _Std430ArrayStride;
            }
        };

        //                                         align size | array stride: std140 std430
        using float_ = type<float,    1, 1,         4,    4,                    16,     4>;
        using int_   = type<int32_t,  1, 1,         4,    4,                    16,     4>;
        using uint_  = type<uint32_t, 1, 1,         4,    4,                    16,     4>;
        using vec2   = type<float,    1, 2,         8,    8,                    16,     8>;
        using ivec2  = type<int32_t,  1, 2,         8,    8,                    16,     8>;
        using uvec2  = type<uint32_t, 1, 2,         8,    8,                    16,     8>;
        using vec3   = type<float,    1, 3,        16,   12,                    16,    16>;
        using ivec3  = type<int32_t,  1, 3,        16,   12,                    16,    16>;
        using uvec3  = type<uint32_t, 1, 3,        16,   12,                    16,    16>;
        using vec4   = type<float,    1, 4,        16,   16,                    16,    16>;
        using ivec4  = type<int32_t,  1, 4,        16,   16,                    16,    16>;
        using uvec4  = type<uint32_t, 1, 4,        16,   16,                    16,    16>;
        using mat3   = type<float,    3, 3,        16,   48,                    48,    48>; // 3 vec3 columns, each padded to 16
        using mat4   = type<float,    4, 4,        16,   64,                    64,    64>;
    }

    constexpr size_t round_up(const size_t value, const size_t alignment) noexcept {
        return (value + alignment - 1) / alignment * alignment;
    }

    // A struct aligns to its most-aligned member; std140 rounds that up to 16.
    constexpr size_t struct_alignment(const size_t max_member_alignment, const layout_rule rule) noexcept {
        return rule == layout_rule::std140 ? round_up(max_member_alignment, kVec4Alignment) : max_member_alignment;
    }

    // A struct's size is padded to a multiple of its alignment, which also makes it the stride of a struct array.
    constexpr size_t struct_size(const size_t members_end, const size_t alignment) noexcept {
        return round_up(members_end, alignment);
    }
}
