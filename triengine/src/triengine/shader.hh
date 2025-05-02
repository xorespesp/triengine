#pragma once
#include <triengine/common.h>
#include <triengine/utility/noncopyable.hh>
#include <glad/glad.h>

#include <initializer_list>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <string>

namespace triengine
{
    enum class shader_object_type : GLenum
    {
        vertex = GL_VERTEX_SHADER,
        fragment = GL_FRAGMENT_SHADER,
        geometry = GL_GEOMETRY_SHADER,
    };

    class shader_program final
        : utility::noncopyable
    {
    private:
        using this_type = shader_program;
        static constexpr GLuint kInvalidProgramID{ 0u };

        class shader_object final
            : utility::noncopyable
        {
        private:
            static constexpr GLuint kInvalidShaderID{ 0u };

        private:
            shader_object_type _type;
            GLuint _shader_id{ kInvalidShaderID }; // shader object id

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

    private:
        GLuint _program_id{ kInvalidProgramID }; // program id
        std::vector<shader_object> _attached_shaders;
        std::unordered_map<std::string, GLint> _uniforms_cache;

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

        GLint get_uniform(const std::string& var_name) const;

        //
        // Utility uniform functions
        //

        void set_uniform_int(const std::string& var_name, int value) const;
        void set_uniform_bool(const std::string& var_name, bool value) const;
        void set_uniform_float(const std::string& var_name, float value) const;
        void set_uniform_vec2(const std::string& var_name, float v0, float v1) const;
        void set_uniform_vec3(const std::string& var_name, float v0, float v1, float v2) const;
        void set_uniform_vec4(const std::string& var_name, float v0, float v1, float v2, float v3) const;

        // Eigen helpers
        void set_uniform_vec2(const std::string& var_name, const Eigen::Ref<const Eigen::Vector2f>& value) const;
        void set_uniform_vec3(const std::string& var_name, const Eigen::Ref<const Eigen::Vector3f>& value) const;
        void set_uniform_vec4(const std::string& var_name, const Eigen::Ref<const Eigen::Vector4f>& value) const;
        void set_uniform_mat2(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix2f>& value) const;
        void set_uniform_mat3(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix3f>& value) const;
        void set_uniform_mat4(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix4f>& value) const;

    private:

        void _attach_shader(shader_object&& new_shader);

        // Build uniform variables cache after linking
        // Ref: https://stackoverflow.com/a/20417594
        void _build_uniforms_cache();

    }; // class

} // namespace