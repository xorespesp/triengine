#include "shader_block_layout.hh"

// Compile-time checks of the layout rules.
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

        // Shader block structs: offsets the GLSL compiler must assign to the same declarations.

        // struct point_light { vec3 position; float intensity; vec3 color; }
        struct point_light {
            vec3_f32 position;
            float intensity;
            vec3_f32 color;
            TRIENGINE_SHADER_BLOCK_FIELDS(point_light, position, intensity, color)
        };
        static_assert(block_layout<std430, point_light>::offset_of<&point_light::position>() == 0);
        static_assert(block_layout<std430, point_light>::offset_of<&point_light::intensity>() == 12); // fills the gap after the vec3
        static_assert(block_layout<std430, point_light>::offset_of<&point_light::color>() == 16);
        static_assert(block_layout<std430, point_light>::size() == 32 && block_layout<std140, point_light>::size() == 32);

        // struct { float a; float b[4]; }: std140 pads every array element to 16
        struct float_array {
            float a;
            std::array<float, 4> b;
            TRIENGINE_SHADER_BLOCK_FIELDS(float_array, a, b)
        };
        static_assert(block_layout<std140, float_array>::offset_of<&float_array::b>() == 16 && block_layout<std140, float_array>::size() == 80);
        static_assert(block_layout<std430, float_array>::offset_of<&float_array::b>() == 4 && block_layout<std430, float_array>::size() == 20);

        // struct { mat3 m; float f; }: a mat3 takes 48 bytes
        struct matrix_then_float {
            mat3_f32 m;
            float f;
            TRIENGINE_SHADER_BLOCK_FIELDS(matrix_then_float, m, f)
        };
        static_assert(block_layout<std430, matrix_then_float>::offset_of<&matrix_then_float::f>() == 48 && block_layout<std430, matrix_then_float>::size() == 64);

        // struct { float a; point_light light; float b; }: a nested struct aligns to 16 and is padded to 32
        struct nested {
            float a;
            point_light light;
            float b;
            TRIENGINE_SHADER_BLOCK_FIELDS(nested, a, light, b)
        };
        static_assert(block_layout<std430, nested>::offset_of<&nested::light>() == 16 && block_layout<std430, nested>::offset_of<&nested::b>() == 48);
        static_assert(block_layout<std430, nested>::size() == 64);

        // struct { float a; float b; } under std140 aligns to 16, so an array of it has stride 16
        struct two_floats {
            float a;
            float b;
            TRIENGINE_SHADER_BLOCK_FIELDS(two_floats, a, b)
        };
        struct two_floats_array {
            std::array<two_floats, 3> items;
            TRIENGINE_SHADER_BLOCK_FIELDS(two_floats_array, items)
        };
        static_assert(block_layout<std140, two_floats_array>::size() == 48 && block_layout<std430, two_floats_array>::size() == 24);

        // buffer Lights { uint count; point_light lights[]; }
        struct lights_block {
            uint32_t count;
            std::vector<point_light> lights;
            TRIENGINE_SHADER_BLOCK_FIELDS(lights_block, count, lights)
        };
        static_assert(block_layout<std430, lights_block>::offset_of<&lights_block::lights>() == 16);
        static_assert(block_layout<std430, lights_block>::size() == 16 && block_layout<std430, lights_block>::runtime_array_stride() == 32);
        static_assert(block_layout<std430, lights_block>::size_for(3) == 112);
    }
}
