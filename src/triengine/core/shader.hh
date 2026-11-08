#pragma once
#include <triengine/common.h>
#include <triengine/utility/noncopyable.hh>
#include <glad/gl.h>

#include <deque>
#include <initializer_list>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <string>
#include <string_view>

namespace triengine::core
{
    enum class shader_object_type : GLenum
    {
        vertex = GL_VERTEX_SHADER,
        fragment = GL_FRAGMENT_SHADER,
        geometry = GL_GEOMETRY_SHADER,
        compute = GL_COMPUTE_SHADER,
    };

    static constexpr GLuint kInvalidGLShaderProgramID{ 0u };
    static constexpr GLuint kInvalidGLShaderObjectID{ 0u };

    class shader_program final
        : utility::noncopyable
    {
    private:
        using this_type = shader_program;

        class shader_object final
            : utility::noncopyable
        {
        private:

        private:
            shader_object_type _type;
            GLuint _shader_id{ kInvalidGLShaderObjectID }; // shader object id

        public:
            shader_object(
                shader_object_type shader_type,
                std::initializer_list<const GLchar*> shader_sources
            );

            shader_object(
                shader_object_type shader_type,
                const std::filesystem::path& shader_file_path
            );

            ~shader_object();

            shader_object(shader_object&& rhs) noexcept;
            shader_object& operator=(shader_object&& rhs) noexcept;

            constexpr shader_object_type type() const noexcept;
            constexpr GLuint id() const noexcept;
            constexpr bool is_valid() const noexcept;

        }; // class

    public:
        shader_program();
        ~shader_program();

        shader_program(shader_program&& rhs) noexcept;
        shader_program& operator=(shader_program&& rhs) noexcept;

        GLuint id() const noexcept;

        bool is_valid() const noexcept;

        this_type& attach_vertex_shader(std::initializer_list<const GLchar*> shader_sources);
        this_type& attach_vertex_shader(const std::filesystem::path& shader_file_path);

        this_type& attach_fragment_shader(std::initializer_list<const GLchar*> shader_sources);
        this_type& attach_fragment_shader(const std::filesystem::path& shader_file_path);

        this_type& attach_geometry_shader(std::initializer_list<const GLchar*> shader_sources);
        this_type& attach_geometry_shader(const std::filesystem::path& shader_file_path);

        void link();
        void use();
        void destroy() noexcept;

        GLint get_uniform_location(std::string_view uniform_name) const;
        GLint get_subroutine_uniform_location(shader_object_type stage_type, std::string_view uniform_name) const;

        //
        // Utility uniform functions
        //

        const this_type& set_uniform_int(std::string_view uniform_name, int value) const;
        const this_type& set_uniform_bool(std::string_view uniform_name, bool value) const;
        const this_type& set_uniform_float(std::string_view uniform_name, float value) const;
        const this_type& set_uniform_vec2(std::string_view uniform_name, float v0, float v1) const;
        const this_type& set_uniform_vec3(std::string_view uniform_name, float v0, float v1, float v2) const;
        const this_type& set_uniform_vec4(std::string_view uniform_name, float v0, float v1, float v2, float v3) const;

        // Eigen helpers
        const this_type& set_uniform_vec2(std::string_view uniform_name, const Eigen::Ref<const Eigen::Vector2f>& value) const;
        const this_type& set_uniform_vec3(std::string_view uniform_name, const Eigen::Ref<const Eigen::Vector3f>& value) const;
        const this_type& set_uniform_vec4(std::string_view uniform_name, const Eigen::Ref<const Eigen::Vector4f>& value) const;
        const this_type& set_uniform_mat2(std::string_view uniform_name, const Eigen::Ref<const Eigen::Matrix2f>& value) const;
        const this_type& set_uniform_mat3(std::string_view uniform_name, const Eigen::Ref<const Eigen::Matrix3f>& value) const;
        const this_type& set_uniform_mat4(std::string_view uniform_name, const Eigen::Ref<const Eigen::Matrix4f>& value) const;

        // Sets the active subroutine function for a specific subroutine uniform in a given shader stage.
        void set_active_subroutine(
            shader_object_type stage_type,
            std::string_view subroutine_uniform_name,
            GLuint subroutine_function_index
        );
        
        void set_active_subroutine(
            shader_object_type stage_type,
            std::string_view subroutine_uniform_name,
            std::string_view subroutine_function_name
        );

    private:
        // Name-keyed cache that is looked up by `std::string_view` without building a `std::string`.
        // The keys view the names in `_names`; a deque never relocates its elements, so they stay valid
        // as names are added and when the cache is moved. Copying would leave the keys viewing the source.
        template <class _Value>
        class name_cache final
        {
        public:
            name_cache() = default;
            name_cache(const name_cache&) = delete;
            name_cache& operator=(const name_cache&) = delete;
            name_cache(name_cache&&) noexcept = default;
            name_cache& operator=(name_cache&&) noexcept = default;

            // `false` if `name` is already cached.
            bool insert(const std::string_view name, const _Value value)
            {
                if (_values.count(name) != 0) {
                    return false;
                }
                const std::string& owned_name = _names.emplace_back(name);
                _values.emplace(owned_name, value);
                return true;
            }

            // `nullptr` if `name` is not cached.
            const _Value* find(const std::string_view name) const
            {
                const auto it = _values.find(name);
                return it != _values.end() ? &it->second : nullptr;
            }

            void clear() noexcept
            {
                _values.clear();
                _names.clear();
            }

        private:
            std::deque<std::string> _names;
            std::unordered_map<std::string_view, _Value> _values;
        };

    private:
        void _attach_shader(shader_object&& new_shader);

        // Build uniform variables cache after linking
        // Ref: https://stackoverflow.com/a/20417594
        void _build_uniforms_location_cache();

        // Build subroutine uniform variables cache after linking
        void _build_subroutine_uniforms_cache();

    private:
        GLuint _program_id{ kInvalidGLShaderProgramID }; // program id
        std::vector<shader_object> _attached_shaders;
        name_cache<GLint/* uniform location */> _uniforms_location_map;

        ////////////////////////////////////////////////////////////////////////////////
        // --- Subroutine Cache Members ---
        
        // Stores the currently selected subroutine function indices for each location, per shader stage
        std::unordered_map<
            GLenum/* shader stage */, 
            std::vector<GLuint>/* index: uniform location, value: function index, size: total active subroutine uniforms in current stage */
        > _subroutine_function_indices_vector_map;

        std::unordered_map<
            GLenum/* shader stage */, 
            name_cache<GLint/* uniform location */> // keyed by subroutine uniform name
        > _subroutine_uniforms_name_location_map;

        std::unordered_map<
            GLenum/* shader stage */, 
            name_cache<GLuint/* function index */> // keyed by subroutine function name
        > _subroutine_functions_name_index_map;

        std::unordered_map<
            GLenum/* shader stage */,
            std::unordered_map<GLuint/* function index */, std::string/* subroutine function name */>
        > _subroutine_functions_index_name_map;

        ////////////////////////////////////////////////////////////////////////////////

    }; // class

} // namespace