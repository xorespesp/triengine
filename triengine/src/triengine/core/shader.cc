#include "shader.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <fstream>
#include <sstream>

namespace triengine::core
{
    // compile from source string
    shader_program::shader_object::shader_object(
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

    // compile from source file
    shader_program::shader_object::shader_object(
        shader_object_type shader_type,
        const std::filesystem::path& shader_file_path)
        : _type{ shader_type }
    {
        std::string shader_file_content;

        try
        {
            std::ifstream f;
            f.exceptions(std::ifstream::failbit | std::ifstream::badbit); // ensure ifstream objects can throw exceptions
            f.open(shader_file_path);
            std::stringstream ss;
            ss << f.rdbuf();
            shader_file_content = ss.str();
            f.close();
        }
        catch (const std::ifstream::failure& e)
        {
            TRIENGINE_ERROR("failed to read shader file: %s (file path: %s)"
                , e.what()
                , shader_file_path.string().c_str()
            );
        }

        // NOTE: `glCreateShader()` returns 0 if an error occurs creating the shader object.
        _shader_id = ::glCreateShader(static_cast<std::underlying_type_t<shader_object_type>>(shader_type));
        if (!_shader_id) {
            TRIENGINE_PANIC("Failed to create shader object");
        }

        const std::initializer_list<const GLchar*> shader_sources = { shader_file_content.c_str() };
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

    shader_program::shader_object::~shader_object() {
        if (this->is_valid()) {
            ::glDeleteShader(_shader_id);
        }
    }

    shader_program::shader_object::shader_object(shader_object&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    shader_program::shader_object& shader_program::shader_object::operator=(shader_object&& rhs) noexcept
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

    constexpr shader_object_type shader_program::shader_object::type() const noexcept {
        return _type;
    }

    constexpr GLuint shader_program::shader_object::id() const noexcept {
        return _shader_id;
    }

    constexpr bool shader_program::shader_object::is_valid() const noexcept {
        return _shader_id != kInvalidShaderID;
    }

    ////////////////////////////////////////////////////////////////////////////////////////

    shader_program::shader_program()
    { }

    shader_program::~shader_program()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    shader_program::shader_program(shader_program&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    shader_program& shader_program::operator=(shader_program&& rhs) noexcept
    {
        if (this != &rhs) {
            if (this->is_valid()) {
                this->destroy();
            }
            std::swap(_program_id, rhs._program_id);
            std::swap(_attached_shaders, rhs._attached_shaders);
            std::swap(_uniforms_cache, rhs._uniforms_cache);
        }

        return *this;
    }

    GLuint shader_program::id() const noexcept {
        return _program_id;
    }

    bool shader_program::is_valid() const noexcept {
        return _program_id != kInvalidProgramID;
    }

    shader_program::this_type& shader_program::attach_vertex_shader(std::initializer_list<const GLchar*> shader_sources)
    {
        this->_attach_shader(shader_object{ shader_object_type::vertex, shader_sources });
        return *this;
    }

    shader_program::this_type& shader_program::attach_vertex_shader(const std::filesystem::path& shader_file_path)
    {
        this->_attach_shader(shader_object{ shader_object_type::vertex, shader_file_path });
        return *this;
    }

    shader_program::this_type& shader_program::attach_fragment_shader(std::initializer_list<const GLchar*> shader_sources)
    {
        this->_attach_shader(shader_object{ shader_object_type::fragment, shader_sources });
        return *this;
    }

    shader_program::this_type& shader_program::attach_fragment_shader(const std::filesystem::path& shader_file_path)
    {
        this->_attach_shader(shader_object{ shader_object_type::fragment, shader_file_path });
        return *this;
    }

    shader_program::this_type& shader_program::attach_geometry_shader(std::initializer_list<const GLchar*> shader_sources)
    {
        this->_attach_shader(shader_object{ shader_object_type::geometry, shader_sources });
        return *this;
    }

    shader_program::this_type& shader_program::attach_geometry_shader(const std::filesystem::path& shader_file_path)
    {
        this->_attach_shader(shader_object{ shader_object_type::geometry, shader_file_path });
        return *this;
    }

    void shader_program::link()
    {
        TRIENGINE_ASSERT(this->is_valid());

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

    void shader_program::use()
    {
        TRIENGINE_ASSERT(this->is_valid());
        ::glUseProgram(_program_id);
    }

    void shader_program::destroy() noexcept
    {
        if (this->is_valid())
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

    GLint shader_program::get_uniform(const std::string& var_name) const
    {
        TRIENGINE_ASSERT(this->is_valid());

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

    void shader_program::set_uniform_int(const std::string& var_name, int value) const {
        GLCall(::glUniform1i(this->get_uniform(var_name), value));
    }
    void shader_program::set_uniform_bool(const std::string& var_name, bool value) const {
        GLCall(::glUniform1i(this->get_uniform(var_name), static_cast<int>(value)));
    }
    void shader_program::set_uniform_float(const std::string& var_name, float value) const {
        GLCall(::glUniform1f(this->get_uniform(var_name), value));
    }
    void shader_program::set_uniform_vec2(const std::string& var_name, float v0, float v1) const {
        GLCall(::glUniform2f(this->get_uniform(var_name), v0, v1);)
    }
    void shader_program::set_uniform_vec3(const std::string& var_name, float v0, float v1, float v2) const {
        GLCall(::glUniform3f(this->get_uniform(var_name), v0, v1, v2));
    }
    void shader_program::set_uniform_vec4(const std::string& var_name, float v0, float v1, float v2, float v3) const {
        GLCall(::glUniform4f(this->get_uniform(var_name), v0, v1, v2, v3));
    }

    // Eigen helpers
    void shader_program::set_uniform_vec2(const std::string& var_name, const Eigen::Ref<const Eigen::Vector2f>& value) const {
        GLCall(::glUniform2fv(this->get_uniform(var_name), 1, value.data()));
    }
    void shader_program::set_uniform_vec3(const std::string& var_name, const Eigen::Ref<const Eigen::Vector3f>& value) const {
        GLCall(::glUniform3fv(this->get_uniform(var_name), 1, value.data()));
    }
    void shader_program::set_uniform_vec4(const std::string& var_name, const Eigen::Ref<const Eigen::Vector4f>& value) const {
        GLCall(::glUniform4fv(this->get_uniform(var_name), 1, value.data()));
    }
    void shader_program::set_uniform_mat2(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix2f>& value) const {
        GLCall(::glUniformMatrix2fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
    }
    void shader_program::set_uniform_mat3(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix3f>& value) const {
        GLCall(::glUniformMatrix3fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
    }
    void shader_program::set_uniform_mat4(const std::string& var_name, const Eigen::Ref<const Eigen::Matrix4f>& value) const {
        GLCall(::glUniformMatrix4fv(this->get_uniform(var_name), 1, GL_FALSE, value.data()));
    }

    void shader_program::_attach_shader(shader_object&& new_shader) {
        if (!this->is_valid()) {
            // NOTE: glCreateProgram() returns 0 if an error occurs creating the program object.
            _program_id = ::glCreateProgram();
            if (!_program_id) {
                TRIENGINE_PANIC("Failed to create shader program");
            }
        }
        GLCall(::glAttachShader(_program_id, new_shader.id()));
        _attached_shaders.emplace_back(std::move(new_shader));
    }

    void shader_program::_build_uniforms_cache()
    {
        _uniforms_cache.clear();

        // Retrieves the longest length of the uniform variable name 
        // among uniform variables (including null terminator)
        GLint max_active_uniform_var_name_len{};
        GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_active_uniform_var_name_len));

        // Get number of active uniforms
        GLint num_of_active_uniforms{};
        GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORMS, &num_of_active_uniforms));

        TRIENGINE_TRACE("----- num_of_active_uniforms = %d", num_of_active_uniforms);
        for (GLint idx = 0; idx < num_of_active_uniforms; ++idx)
        {
            std::string var_name_buff; // variable name in GLSL
            var_name_buff.resize(max_active_uniform_var_name_len);

            GLsizei var_name_len{};    // name length (excluding the null terminator)
            GLint var_size{};          // data size of the variable
            GLenum var_type{};         // data type of the variable (float, vec3 or mat4, etc)

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

} // namespace