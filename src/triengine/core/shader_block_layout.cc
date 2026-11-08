#include "shader_block_layout.hh"

// Compile-time checks of the struct rules.
namespace triengine::core::shader_block
{
    namespace
    {
        constexpr layout_rule std140 = layout_rule::std140;
        constexpr layout_rule std430 = layout_rule::std430;

        // Instantiates every table entry, so the typo checks in `glsl::type` run.
        template <class... _Types>
        constexpr bool all_instantiated() noexcept { return ((_Types::size > 0) && ...); }
        static_assert(all_instantiated<
            glsl::float_, glsl::int_, glsl::uint_,
            glsl::vec2, glsl::ivec2, glsl::uvec2,
            glsl::vec3, glsl::ivec3, glsl::uvec3,
            glsl::vec4, glsl::ivec4, glsl::uvec4,
            glsl::mat3, glsl::mat4>());

        // struct { float a; float b; }  ->  std140: alignment 16, size 16 / std430: alignment 4, size 8
        static_assert(struct_alignment(4, std140) == 16 && struct_size(8, struct_alignment(4, std140)) == 16);
        static_assert(struct_alignment(4, std430) == 4 && struct_size(8, struct_alignment(4, std430)) == 8);

        // struct { vec3 a; }  ->  12 bytes of members, padded to 16 under both rules
        static_assert(struct_size(12, struct_alignment(16, std140)) == 16);
        static_assert(struct_size(12, struct_alignment(16, std430)) == 16);

        // struct { mat4 a; vec2 b; }  ->  members end at 72: std140 and std430 both pad to 80
        static_assert(struct_size(72, struct_alignment(16, std140)) == 80);
        static_assert(struct_size(72, struct_alignment(16, std430)) == 80);
    }
}
