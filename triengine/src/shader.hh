#pragma once
#include "common.h"
#include "misc/debug_utils.hh"
#include "misc/gl_utils.hh"

#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

#include <initializer_list>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <array>
#include <string>

namespace triengine
{
    enum class shader_object_type : GLenum
    {
        vertex = GL_VERTEX_SHADER,
        fragment = GL_FRAGMENT_SHADER,
        geometry = GL_GEOMETRY_SHADER,
    };

    class shader_object final
    {
    private:
        static constexpr GLuint kInvalidShaderID{ 0u };

    private:
        shader_object_type _type;
        GLuint _shader_id{ kInvalidShaderID }; // shader id

    public:
        shader_object(
            shader_object_type shader_type,
            std::initializer_list<const GLchar*> shader_sources)
            : _type{ shader_type }
        {
            // NOTE: `glCreateShader()` returns 0 if an error occurs creating the shader object.
            _shader_id = ::glCreateShader(static_cast<std::underlying_type_t<shader_object_type>>(shader_type));
            if (!_shader_id) {
                TRIENGINE_PANIC("Failed to create shader object");
            }

            ::glShaderSource(_shader_id, static_cast<GLsizei>(shader_sources.size()), shader_sources.begin(), nullptr);
            ::glCompileShader(_shader_id);

            // validate shader
            GLint gl_success = GL_FALSE;
            ::glGetShaderiv(_shader_id, GL_COMPILE_STATUS, &gl_success);
            if (!gl_success) {
                GLint info_log_len{};
                ::glGetShaderiv(_shader_id, GL_INFO_LOG_LENGTH, &info_log_len);

                std::string info_log;
                info_log.resize(info_log_len);
                ::glGetShaderInfoLog(_shader_id, info_log_len, &info_log_len, info_log.data());

                TRIENGINE_PANIC("Failed to compile shader object: %s", info_log.c_str());
            }
        }

        shader_object(
            shader_object_type shader_type,
            const GLchar* shader_source)
            : shader_object(shader_type, { shader_source })
        { }

        ~shader_object() {
            if (this->is_valid()) {
                ::glDeleteShader(_shader_id);
            }
        }

        shader_object(shader_object&& rhs) noexcept
        {
            *this = std::move(rhs);
        }

        shader_object& operator=(shader_object&& rhs) noexcept
        {
            if (this != &rhs) {
                // Delete old shader if exists
                if (this->is_valid()) {
                    ::glDeleteShader(_shader_id);
                }
                
                _type = rhs._type;
                _shader_id = rhs._shader_id;
                rhs._shader_id = kInvalidShaderID;
            }
            return *this;
        }

        shader_object(const shader_object&) = delete;
        shader_object& operator=(const shader_object&) = delete;

        constexpr shader_object_type type() const noexcept {
            return _type;
        }

        constexpr GLuint id() const noexcept {
            return _shader_id;
        }

        constexpr bool is_valid() const noexcept {
            return _shader_id != kInvalidShaderID;
        }

    }; // class

    // Refs:
    // https://github.com/quink-black/gles3-android/blob/master/app/src/main/cpp/opengl-helper.h
    // https://github.com/xorespesp/meshview-demo/blob/main/src/meshview/internal/shader.hpp
    // LearnOpenGL/includes/learnopengl/shader_m.h
    // Azure-Kinect-Sensor-SDK/tools/k4aviewer/openglhelpers.h
    class shader_program final
    {
    private:
        static constexpr GLuint kInvalidProgramID{ 0u };

    private:
        GLuint _program_id{ kInvalidProgramID }; // program id
        std::vector<shader_object> _attached_shaders;
        std::unordered_map<std::string, GLint> _uniforms_cache;

    public:
        shader_program() = default;
        ~shader_program() {
            if (this->is_created()) {
                this->destroy();
            }
        }

        shader_program(shader_program&& rhs) noexcept
        {
            *this = std::move(rhs);
        }

        shader_program& operator=(shader_program&& rhs) noexcept
        {
            if (this != &rhs) {
                if (this->is_created()) {
                    this->destroy();
                }
                std::swap(_program_id, rhs._program_id);
                std::swap(_attached_shaders, rhs._attached_shaders);
                std::swap(_uniforms_cache, rhs._uniforms_cache);
            }

            return *this;
        }

        shader_program(const shader_program&) = delete;
        shader_program& operator=(const shader_program&) = delete;

        GLuint id() const noexcept {
            return _program_id;
        }

        bool is_created() const noexcept {
            return _program_id != kInvalidProgramID;
        }

        void create() {
            TRIENGINE_ASSERT(!this->is_created());
            _program_id = ::glCreateProgram(); // NOTE: glCreateProgram() returns 0 if an error occurs creating the program object.
            if (!_program_id) {
                TRIENGINE_PANIC("Failed to create shader program");
            }
        }

        void attach_vertex_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->_attach_shader(shader_object{ shader_object_type::vertex, shader_sources });
        }

        void attach_fragment_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->_attach_shader(shader_object{ shader_object_type::fragment, shader_sources });
        }
        
        void attach_geometry_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->_attach_shader(shader_object{ shader_object_type::geometry, shader_sources });
        }

        void link()
        {
            TRIENGINE_ASSERT(this->is_created());

            ::glLinkProgram(_program_id);

            // validate shader program
            GLint gl_success = GL_FALSE;
            ::glGetProgramiv(_program_id, GL_LINK_STATUS, &gl_success);
            if (!gl_success) {
                GLint info_log_len{};
                ::glGetProgramiv(_program_id, GL_INFO_LOG_LENGTH, &info_log_len);

                std::string info_log;
                info_log.resize(info_log_len);
                ::glGetProgramInfoLog(_program_id, info_log_len, &info_log_len, info_log.data());

                TRIENGINE_PANIC("Failed to link shader program: %s", info_log.c_str());
            }

            //// validate shader program (optional)
            // glValidateProgram(_program_id);
            
            // Build uniform cache
            this->_build_uniforms_cache();
        }

        void use()
        {
            TRIENGINE_ASSERT(this->is_created());
            ::glUseProgram(_program_id);
        }

        void destroy() noexcept
        {
            if (this->is_created())
            {
                // Unbind if this program is currently in use
                GLint curr_prog_id{};
                ::glGetIntegerv(GL_CURRENT_PROGRAM, &curr_prog_id);
                if (_program_id == static_cast<GLuint>(curr_prog_id)) {
                    ::glUseProgram(0);
                }

                // Detach shaders (optional but clean)
                for (auto& shdr : _attached_shaders) {
                    ::glDetachShader(_program_id, shdr.id());
                }

                // Delete program
                ::glDeleteProgram(_program_id);
            }

            _program_id = kInvalidProgramID;
            _attached_shaders.clear();
            _uniforms_cache.clear();
        }

        // Get uniform location (cached)
        inline GLint get_uniform(const std::string& var_name) const
        {
            TRIENGINE_ASSERT(this->is_created());

            // use cache if possible
            const auto it = _uniforms_cache.find(var_name);
            if (it != _uniforms_cache.end()) {
                return it->second;
            }

            // if not found in cache, do a direct lookup
            // TODO: Optionally you can handle -1 gracefully instead of assert
            const GLint uloc = ::glGetUniformLocation(_program_id, var_name.c_str());
            TRIENGINE_ASSERT(uloc != -1);
            return uloc;
        }

        //
        // Utility uniform functions
        //

        inline void set_uniform_int(const std::string& var_name, int value) const {
            GLCall(::glUniform1i(this->get_uniform(var_name), value));
        }
        inline void set_uniform_bool(const std::string& var_name, bool value) const {
            GLCall(::glUniform1i(this->get_uniform(var_name), static_cast<int>(value)));
        }
        inline void set_uniform_float(const std::string& var_name, float value) const {
            GLCall(::glUniform1f(this->get_uniform(var_name), value));
        }
        inline void set_uniform_vec2(const std::string& var_name, float v0, float v1) const {
            GLCall(::glUniform2f(this->get_uniform(var_name), v0, v1);)
        }
        inline void set_uniform_vec3(const std::string& var_name, float v0, float v1, float v2) const {
            GLCall(::glUniform3f(this->get_uniform(var_name), v0, v1, v2));
        }
        inline void set_uniform_vec4(const std::string& var_name, float v0, float v1, float v2, float v3) {
            GLCall(::glUniform4f(this->get_uniform(var_name), v0, v1, v2, v3));
        }

        // Eigen helpers
        inline void set_uniform_vec2(const std::string& var_name, const Eigen::Ref<const vec2_f32>& value) const {
            GLCall(::glUniform2fv(this->get_uniform(var_name), 1, value.data()));
        }
        inline void set_uniform_vec3(const std::string& var_name, const Eigen::Ref<const vec3_f32>& value) const {
            GLCall(::glUniform3fv(this->get_uniform(var_name), 1, value.data()));
        }
        inline void set_uniform_vec4(const std::string& var_name, const Eigen::Ref<const vec4_f32>& value) const {
            GLCall(::glUniform4fv(this->get_uniform(var_name), 1, value.data()));
        }
        inline void set_uniform_mat2(const std::string& var_name, const Eigen::Ref<const mat2_f32>& value) const {
            GLCall(::glUniformMatrix2fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
        }
        inline void set_uniform_mat3(const std::string& var_name, const Eigen::Ref<const mat3_f32>& value) const {
            GLCall(::glUniformMatrix3fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
        }
        inline void set_uniform_mat4(const std::string& var_name, const Eigen::Ref<const mat4_f32>& value) const {
            GLCall(::glUniformMatrix4fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
        }

    private:

        void _attach_shader(shader_object&& new_shader) {
            TRIENGINE_ASSERT(this->is_created());
            GLCall(::glAttachShader(_program_id, new_shader.id()));
            _attached_shaders.emplace_back(std::move(new_shader));
        }

        // Build uniform variables cache after linking
        // Ref: https://stackoverflow.com/a/20417594
        void _build_uniforms_cache();

    }; // class

} // namespace