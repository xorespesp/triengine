#pragma once
#include <triengine/common.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

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
            static_assert(sizeof(_Component) == 4, "GLSL components are 32-bit");

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
    } // namespace

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

    // ---------------------------------------------------------------------------------
    // Shader block structs: plain C++ structs whose fields are listed with `TRIENGINE_SHADER_BLOCK_FIELDS`.
    //
    //   struct point_light {
    //       vec3_f32 position;
    //       float intensity;
    //       vec3_f32 color;
    //       TRIENGINE_SHADER_BLOCK_FIELDS(point_light, position, intensity, color)
    //   };
    //
    // Field types and their GLSL types:
    //   `float`, `int32_t`, `uint32_t`                              float, int, uint
    //   Eigen column vectors of 2 to 4 of them                      vec*, ivec*, uvec*
    //   Eigen column-major 3x3 / 4x4 float matrices                 mat3, mat4
    //   `color3_f32`, `color4_f32`                                  vec3, vec4
    //   `std::array<T, N>`                                          T[N] (no arrays of arrays)
    //   another shader block struct                                 a nested struct
    //   `std::vector<T>` as the last field of the outermost struct  T[], the runtime-sized array of a shader storage block
    // List the fields in GLSL declaration order, with the GLSL member names.
    // Each field once, and only fields declared in the struct itself: shader block structs do not inherit.
    // Put the macro in a public section (anywhere in a `struct`); a private one does not compile.
    // The macro keeps the struct an aggregate.
    // ---------------------------------------------------------------------------------

    // Layout of `_Block` under `_Rule`, and its serialization. Everything is static:
    //
    //   using lights_layout = block_layout<layout_rule::std430, lights_block>;
    //   static_assert(lights_layout::offset_of<&lights_block::lights>() == 16);
    //   lights_layout::serialize(block, bytes);
    template <layout_rule _Rule, class _Block>
    class block_layout final
    {
    public:
        block_layout() = delete;

        static constexpr layout_rule rule = _Rule;

        // Bytes of the block; with a runtime-sized array, the bytes before it.
        static constexpr size_t size() noexcept;

        // Distance between runtime-sized array elements; 0 without such an array.
        static constexpr size_t runtime_array_stride() noexcept;

        // Bytes of the block with `element_count` runtime-sized array elements.
        static constexpr size_t size_for(size_t element_count) noexcept;

        // Offset of a field of `_Block`, e.g. `offset_of<&lights_block::lights>()`.
        template <auto _FieldPointer>
        static constexpr size_t offset_of() noexcept;

        // Padding is written as zeros, so equal values always give equal bytes.

        // Resizes `bytes` to the whole block, runtime-sized array included.
        static void serialize(const _Block& block, std::vector<std::byte>& bytes);

        // Writes runtime-sized array element `index` into an already sized block.
        template <class _Element>
        static void serialize_element(size_t index, const _Element& element, std::byte* bytes, size_t byte_count);

        // Reads a block back; the runtime-sized array gets as many elements as `byte_count` holds.
        static void deserialize(const std::byte* bytes, size_t byte_count, _Block& block);
    };
}

// Lists the fields of a shader block struct, inside its definition. Up to 32 fields.
#define TRIENGINE_SHADER_BLOCK_FIELDS(STRUCT, ...) \
    static constexpr auto shader_block_fields() noexcept { \
        return ::std::make_tuple(_TRIENGINE_SHADER_BLOCK_FOR_EACH(_TRIENGINE_SHADER_BLOCK_FIELD, STRUCT, __VA_ARGS__)); \
    }

#include "shader_block_layout_impl.hh"
