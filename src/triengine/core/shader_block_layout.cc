#include "shader_block_layout.hh"
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/string_format.hh>

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Driver check: `block_layout::verify` builds the expected variables, `compare_with_driver` queries the driver and compares.
namespace triengine::core::shader_block::detail
{
    void compare_with_driver(
        const uint32_t program,
        const block_type type,
        const std::string_view block_name_sv,
        const uint32_t binding,
        const expected_block& expected)
    {
        using utility::string::c_format;

        // A variable as the driver reports it.
        struct reported_variable {
            GLenum type;
            GLint offset;
            GLint array_size;
            GLint array_stride;
            GLint matrix_stride;
            GLint is_row_major;
            GLint top_level_array_size;   // shader storage blocks only
            GLint top_level_array_stride; // shader storage blocks only
        };

        // GL types a shader block field can have.
        struct gl_type_info {
            GLenum type;
            scalar_type component;
            uint32_t columns;
            uint32_t rows;
            const char* glsl_name;
        };

        static constexpr std::array kGLTypes{
            gl_type_info{ GL_FLOAT,             scalar_type::f32, 1, 1, "float" },
            gl_type_info{ GL_FLOAT_VEC2,        scalar_type::f32, 1, 2, "vec2" },
            gl_type_info{ GL_FLOAT_VEC3,        scalar_type::f32, 1, 3, "vec3" },
            gl_type_info{ GL_FLOAT_VEC4,        scalar_type::f32, 1, 4, "vec4" },
            gl_type_info{ GL_INT,               scalar_type::i32, 1, 1, "int" },
            gl_type_info{ GL_INT_VEC2,          scalar_type::i32, 1, 2, "ivec2" },
            gl_type_info{ GL_INT_VEC3,          scalar_type::i32, 1, 3, "ivec3" },
            gl_type_info{ GL_INT_VEC4,          scalar_type::i32, 1, 4, "ivec4" },
            gl_type_info{ GL_UNSIGNED_INT,      scalar_type::u32, 1, 1, "uint" },
            gl_type_info{ GL_UNSIGNED_INT_VEC2, scalar_type::u32, 1, 2, "uvec2" },
            gl_type_info{ GL_UNSIGNED_INT_VEC3, scalar_type::u32, 1, 3, "uvec3" },
            gl_type_info{ GL_UNSIGNED_INT_VEC4, scalar_type::u32, 1, 4, "uvec4" },
            gl_type_info{ GL_FLOAT_MAT3,        scalar_type::f32, 3, 3, "mat3" },
            gl_type_info{ GL_FLOAT_MAT4,        scalar_type::f32, 4, 4, "mat4" },
        };

        const auto get_gl_type = [](const expected_variable& variable) {
            const auto it = std::find_if(kGLTypes.begin(), kGLTypes.end(), [&](const gl_type_info& info) {
                return info.component == variable.component && info.columns == variable.columns && info.rows == variable.rows;
            });
            return it != kGLTypes.end() ? it->type : GL_NONE;
        };

        // GLSL name of a GL type, or its hex value for a type no shader block field can have.
        const auto get_type_name = [](const GLenum gl_type) {
            const auto it = std::find_if(kGLTypes.begin(), kGLTypes.end(), [&](const gl_type_info& info) { return info.type == gl_type; });
            return it != kGLTypes.end() ? std::string(it->glsl_name) : c_format("0x%04X", gl_type);
        };

        const std::string block_name{ block_name_sv };
        const bool is_storage = type == block_type::shader_storage;
        const char* const block_type_name = is_storage ? "Shader storage block" : "Uniform block";
        const char* const rule_name = expected.rule == layout_rule::std140 ? "std140" : "std430";

        // Misuses that the driver would not point out.
        if (!is_storage && expected.rule == layout_rule::std430) {
            TRIENGINE_PANIC("Uniform block '%s' uses a std430 layout; uniform blocks are std140", block_name.c_str());
        }
        if (!is_storage && expected.has_runtime_array) {
            TRIENGINE_PANIC("Uniform block '%s' ends with a std::vector field; only a shader storage block has a runtime-sized array", block_name.c_str());
        }

        const GLenum block_interface = is_storage ? GL_SHADER_STORAGE_BLOCK : GL_UNIFORM_BLOCK;
        const GLenum variable_interface = is_storage ? GL_BUFFER_VARIABLE : GL_UNIFORM;

        const GLuint block_index = ::glGetProgramResourceIndex(program, block_interface, block_name.c_str()); // No GLCall, GL_INVALID_INDEX possible
        if (block_index == GL_INVALID_INDEX) {
            TRIENGINE_PANIC("%s '%s' is not in program %u: a wrong block name, or no shader of the program uses the block"
                , block_type_name
                , block_name.c_str()
                , program);
        }

        // The block: binding point, required size, variables.
        constexpr std::array<GLenum, 3> kBlockProps{ GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES };
        std::array<GLint, kBlockProps.size()> block_values{};
        GLCall(::glGetProgramResourceiv(program, block_interface, block_index,
            static_cast<GLsizei>(kBlockProps.size()), kBlockProps.data(),
            static_cast<GLsizei>(block_values.size()), nullptr, block_values.data()));
        const GLint gl_binding = block_values[0];
        const GLint gl_data_size = block_values[1];
        const GLint variable_count = block_values[2];

        std::vector<GLint> variable_indices(static_cast<size_t>(variable_count));
        constexpr GLenum kActiveVariables = GL_ACTIVE_VARIABLES;
        GLCall(::glGetProgramResourceiv(program, block_interface, block_index,
            1, &kActiveVariables,
            variable_count, nullptr, variable_indices.data()));

        // Each variable, by name. A block with an instance name prefixes its members with the block name.
        constexpr std::array<GLenum, 9> kVariableProps{
            GL_NAME_LENGTH, GL_TYPE, GL_OFFSET, GL_ARRAY_SIZE, GL_ARRAY_STRIDE, GL_MATRIX_STRIDE, GL_IS_ROW_MAJOR,
            GL_TOP_LEVEL_ARRAY_SIZE, GL_TOP_LEVEL_ARRAY_STRIDE, // the last two exist for buffer variables only
        };
        const GLsizei variable_prop_count = is_storage ? 9 : 7;
        const std::string instance_prefix = block_name + ".";

        std::unordered_map<std::string, reported_variable> reported;
        for (const GLint variable_index : variable_indices)
        {
            std::array<GLint, kVariableProps.size()> values{};
            GLCall(::glGetProgramResourceiv(program, variable_interface, static_cast<GLuint>(variable_index),
                variable_prop_count, kVariableProps.data(),
                variable_prop_count, nullptr, values.data()));

            std::string name(static_cast<size_t>(values[0]), '\0'); // the length counts the null terminator
            GLsizei name_length = 0;
            GLCall(::glGetProgramResourceName(program, variable_interface, static_cast<GLuint>(variable_index),
                values[0], &name_length, name.data()));
            name.resize(static_cast<size_t>(name_length));
            if (name.compare(0, instance_prefix.size(), instance_prefix) == 0) {
                name.erase(0, instance_prefix.size());
            }

            reported.emplace(std::move(name), reported_variable{ // `values` is in `kVariableProps` order
                static_cast<GLenum>(values[1]), values[2], values[3], values[4], values[5], values[6], values[7], values[8]
            });
        }

        // Compare. Every member of a std140 or std430 block is active, so the driver lists them all.
        std::vector<std::string> problems;

        if (gl_binding != static_cast<GLint>(binding)) {
            problems.push_back(c_format("binding point: C++ %u, GL %d", binding, gl_binding));
        }
        if (static_cast<size_t>(gl_data_size) > expected.data_size) {
            problems.push_back(c_format("block size: GL requires %d bytes, C++ lays out %zu", gl_data_size, expected.data_size));
        }

        for (const expected_variable& variable : expected.variables)
        {
            const auto it = reported.find(variable.name);
            if (it == reported.end()) {
                problems.push_back(c_format("`%s`: in TRIENGINE_SHADER_BLOCK_FIELDS but not in GLSL", variable.name.c_str()));
                continue;
            }
            const reported_variable& gl = it->second;
            const auto compare = [&](const char* const property, const size_t cpp_value, const GLint gl_value) {
                if (static_cast<long long>(cpp_value) != static_cast<long long>(gl_value)) {
                    problems.push_back(c_format("`%s` %s: C++ %zu, GL %d", variable.name.c_str(), property, cpp_value, gl_value));
                }
            };

            const GLenum cpp_type = get_gl_type(variable);
            if (cpp_type != gl.type) {
                problems.push_back(c_format("`%s` type: C++ %s, GL %s"
                    , variable.name.c_str()
                    , get_type_name(cpp_type).c_str()
                    , get_type_name(gl.type).c_str()));
            }
            compare("offset", variable.offset, gl.offset);
            compare("array size", variable.array_size, gl.array_size);
            compare("array stride", variable.array_stride, gl.array_stride);
            compare("matrix stride", variable.matrix_stride, gl.matrix_stride);
            if (gl.is_row_major != 0) {
                problems.push_back(c_format("`%s`: row_major in GLSL; shader block matrices are column-major", variable.name.c_str()));
            }
            if (variable.top_level.present) {
                compare("top-level array size", variable.top_level.size, gl.top_level_array_size);
                compare("top-level array stride", variable.top_level.stride, gl.top_level_array_stride);
            }
            reported.erase(it);
        }

        // Whatever the driver reported beyond the C++ list, in block order.
        std::vector<std::pair<std::string, reported_variable>> unlisted(reported.begin(), reported.end());
        std::sort(unlisted.begin(), unlisted.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.second.offset != rhs.second.offset ? lhs.second.offset < rhs.second.offset : lhs.first < rhs.first;
        });
        for (const auto& [name, gl] : unlisted) {
            problems.push_back(c_format("`%s`: in GLSL (offset %d, %s) but not in TRIENGINE_SHADER_BLOCK_FIELDS"
                , name.c_str()
                , gl.offset
                , get_type_name(gl.type).c_str()));
        }

        if (!problems.empty()) {
            std::string message = c_format("%s '%s' (%s) does not match its C++ layout:", block_type_name, block_name.c_str(), rule_name);
            for (const std::string& problem : problems) {
                message += "\n  ";
                message += problem;
            }
            TRIENGINE_PANIC(message);
        }

        TRIENGINE_DEBUG("%s '%s' (%s, binding %u) matches its C++ layout: %zu variables, %d bytes"
            , block_type_name
            , block_name.c_str()
            , rule_name
            , binding
            , expected.variables.size()
            , gl_data_size);
    }

} // namespace

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
