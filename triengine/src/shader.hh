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
#include <list>
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
        static constexpr GLuint kInvalidShaderID{ static_cast<GLuint>(0) };

    private:
        GLuint _shader_id{ kInvalidShaderID }; // shader id

    public:
        shader_object(
            shader_object_type shader_type,
            std::initializer_list<const GLchar*> shader_sources)
        {
            _shader_id = ::glCreateShader(static_cast<std::underlying_type_t<shader_object_type>>(shader_type)); // NOTE: glCreateShader() returns 0 if an error occurs creating the shader object.
            ::glShaderSource(_shader_id, static_cast<GLsizei>(shader_sources.size()), shader_sources.begin(), nullptr);
            ::glCompileShader(_shader_id);

            // validate shader
            GLint gl_success = GL_FALSE;
            ::glGetShaderiv(_shader_id, GL_COMPILE_STATUS, &gl_success);
            if (!gl_success) {
                std::array<char, 512> info_log_buff;
                ::glGetShaderInfoLog(_shader_id, static_cast<GLsizei>(info_log_buff.size()), nullptr, info_log_buff.data());
                TRIENGINE_PANIC("Shader compile error: %s", info_log_buff.data());
            }
        }

        shader_object(
            shader_object_type shader_type,
            const GLchar* shader_source)
            : shader_object(shader_type, { shader_source })
        { }

        ~shader_object() {
            if (_shader_id != kInvalidShaderID) {
                GLCall(::glDeleteShader(_shader_id));
            }
        }

        shader_object(shader_object&& rhs) noexcept
            : _shader_id{ rhs._shader_id }
        {
            rhs._shader_id = kInvalidShaderID;
        }

        shader_object& operator=(shader_object&& rhs) noexcept {
            if (this != &rhs) {
                _shader_id = rhs._shader_id;
                rhs._shader_id = kInvalidShaderID;
            }
            return *this;
        }

        shader_object(const shader_object&) = delete;
        shader_object& operator=(const shader_object&) = delete;

        constexpr GLuint id() const noexcept {
            return _shader_id;
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
        static constexpr GLuint kInvalidProgramID{ static_cast<GLuint>(0) };

    private:
        GLuint _program_id{ kInvalidProgramID }; // program id
        std::list<shader_object> _attached_shaders;
        std::unordered_map<std::string, GLint> _uniforms_cache;

    public:
        shader_program() = default;
        ~shader_program() {
            if (this->is_created()) {
                this->destroy();
            }
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
        }

        void attach_shader(shader_object&& new_shader) {
            TRIENGINE_ASSERT(this->is_created());
            GLCall(::glAttachShader(_program_id, new_shader.id()));
            _attached_shaders.emplace_back(std::move(new_shader));
        }

        void attach_vertex_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->attach_shader(shader_object{ shader_object_type::vertex, shader_sources });
        }

        void attach_fragment_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->attach_shader(shader_object{ shader_object_type::fragment, shader_sources });
        }
        
        void attach_geometry_shader(std::initializer_list<const GLchar*> shader_sources) {
            this->attach_shader(shader_object{ shader_object_type::geometry, shader_sources });
        }

        void link()
        {
            TRIENGINE_ASSERT(this->is_created());

            ::glLinkProgram(_program_id);

            // validate shader program
            GLint gl_success = GL_FALSE;
            ::glGetProgramiv(_program_id, GL_LINK_STATUS, &gl_success);
            if (!gl_success) {
                std::array<char, 512> info_log_buff;
                ::glGetProgramInfoLog(_program_id, static_cast<GLsizei>(info_log_buff.size()), nullptr, info_log_buff.data());
                TRIENGINE_PANIC("Shader program link error: %s", info_log_buff.data());
            }

            //
            // Generate uniforms location cache
            // See: https://stackoverflow.com/a/20417594
            //

            // Retrieves the longest length of the uniform variable name among uniform variables (including null terminator)
            GLint max_active_uniform_var_name_len{};
            GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_active_uniform_var_name_len));

            GLint num_of_active_uniforms{};
            GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORMS, &num_of_active_uniforms));

            TRIENGINE_TRACE("----- num_of_active_uniforms = %d", num_of_active_uniforms);
            for (GLint idx = 0; idx < num_of_active_uniforms; ++idx)
            {
                std::string var_name_buff; // variable name in GLSL
                var_name_buff.resize(max_active_uniform_var_name_len);
                GLsizei var_name_len;      // name length (excluding the null terminator)
                GLint var_size;            // data size of the variable
                GLenum var_type;           // data type of the variable (float, vec3 or mat4, etc)

                // get the name of this uniform
                ::glGetActiveUniform(
                    _program_id,
                    static_cast<GLuint>(idx),
                    static_cast<GLsizei>(var_name_buff.size()),
                    &var_name_len,
                    &var_size,
                    &var_type,
                    var_name_buff.data()
                );

                TRIENGINE_ASSERT(::glGetError() == GL_NO_ERROR);
                var_name_buff.resize(var_name_len);

                const GLint uloc = ::glGetUniformLocation(_program_id, var_name_buff.c_str());
                TRIENGINE_ASSERT(uloc != -1);

                TRIENGINE_TRACE("#%d : uniform_cache[\"%s\"(%llu)] = %d", idx, var_name_buff.c_str(), var_name_buff.size(), uloc);

                // cache for later use
                const auto [_, success] = _uniforms_cache.insert(
                    std::make_pair(std::move(var_name_buff), uloc)
                );

                TRIENGINE_ASSERT(success);
            }
        }


        void use()
        {
            TRIENGINE_ASSERT(this->is_created());
            ::glUseProgram(_program_id);
        }

        void destroy()
        {
            TRIENGINE_ASSERT(this->is_created());

            // Reset the active shader if we're about to delete it
            GLint currentProgramId{};
            ::glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgramId);
            if (_program_id == static_cast<GLuint>(currentProgramId)) {
                ::glUseProgram(0);
            }

            _attached_shaders.clear();
            GLCall(::glDeleteProgram(_program_id));
            _program_id = 0;
        }

        inline GLint get_uniform(const std::string& var_name) const
        {
            TRIENGINE_ASSERT(this->is_created());

            // use cache if possible
            const auto it = _uniforms_cache.find(var_name);
            if (it != _uniforms_cache.end()) {
                return it->second;
            }

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

    }; // class

} // namespace