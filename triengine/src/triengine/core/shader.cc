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
            std::swap(_uniforms_location_map, rhs._uniforms_location_map);
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

        // Build caches
        this->_build_uniforms_location_cache();
        this->_build_subroutine_uniforms_cache();
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
        _uniforms_location_map.clear();
        _subroutine_function_indices_vector_map.clear();
        _subroutine_uniforms_name_location_map.clear();
        _subroutine_functions_name_index_map.clear();
        _subroutine_functions_index_name_map.clear();
    }

    GLint shader_program::get_uniform_location(const std::string& uniform_name) const
    {
        TRIENGINE_ASSERT(this->is_valid());

        // use cache if possible
        const auto it = _uniforms_location_map.find(uniform_name);
        if (it != _uniforms_location_map.end()) {
            return it->second;
        }

        TRIENGINE_PANIC("Uniform '%s' not found in cache."
            , uniform_name.c_str());

        return -1; // Not found
    }

    const shader_program::this_type& shader_program::set_uniform_int(const std::string& uniform_name, int value) const {
        GLCall(::glProgramUniform1i(_program_id, this->get_uniform_location(uniform_name), value));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_bool(const std::string& uniform_name, bool value) const {
        GLCall(::glProgramUniform1i(_program_id, this->get_uniform_location(uniform_name), static_cast<int>(value)));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_float(const std::string& uniform_name, float value) const {
        GLCall(::glProgramUniform1f(_program_id, this->get_uniform_location(uniform_name), value));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_vec2(const std::string& uniform_name, float v0, float v1) const {
        GLCall(::glProgramUniform2f(_program_id, this->get_uniform_location(uniform_name), v0, v1));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_vec3(const std::string& uniform_name, float v0, float v1, float v2) const {
        GLCall(::glProgramUniform3f(_program_id, this->get_uniform_location(uniform_name), v0, v1, v2));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_vec4(const std::string& uniform_name, float v0, float v1, float v2, float v3) const {
        GLCall(::glProgramUniform4f(_program_id, this->get_uniform_location(uniform_name), v0, v1, v2, v3));
        return *this;
    }

    // Eigen helpers
    const shader_program::this_type& shader_program::set_uniform_vec2(const std::string& uniform_name, const Eigen::Ref<const Eigen::Vector2f>& value) const {
        GLCall(::glProgramUniform2fv(_program_id, this->get_uniform_location(uniform_name), 1, value.data()));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_vec3(const std::string& uniform_name, const Eigen::Ref<const Eigen::Vector3f>& value) const {
        GLCall(::glProgramUniform3fv(_program_id, this->get_uniform_location(uniform_name), 1, value.data()));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_vec4(const std::string& uniform_name, const Eigen::Ref<const Eigen::Vector4f>& value) const {
        GLCall(::glProgramUniform4fv(_program_id, this->get_uniform_location(uniform_name), 1, value.data()));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_mat2(const std::string& uniform_name, const Eigen::Ref<const Eigen::Matrix2f>& value) const {
        GLCall(::glProgramUniformMatrix2fv(_program_id, this->get_uniform_location(uniform_name), 1, GL_FALSE, value.data()));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_mat3(const std::string& uniform_name, const Eigen::Ref<const Eigen::Matrix3f>& value) const {
        GLCall(::glProgramUniformMatrix3fv(_program_id, this->get_uniform_location(uniform_name), 1, GL_FALSE, value.data()));
        return *this;
    }
    const shader_program::this_type& shader_program::set_uniform_mat4(const std::string& uniform_name, const Eigen::Ref<const Eigen::Matrix4f>& value) const {
        GLCall(::glProgramUniformMatrix4fv(_program_id, this->get_uniform_location(uniform_name), 1, GL_FALSE, value.data()));
        return *this;
    }

    GLint shader_program::get_subroutine_uniform_location(
        const shader_object_type stage_type,
        const std::string& uniform_name) const
    {
        TRIENGINE_ASSERT(this->is_valid());
        const GLenum stage_enum = static_cast<GLenum>(stage_type);

        const auto stage_cache_it = _subroutine_uniforms_name_location_map.find(stage_enum);
        if (stage_cache_it != _subroutine_uniforms_name_location_map.end()) {
            auto uniform_it = stage_cache_it->second.find(uniform_name);
            if (uniform_it != stage_cache_it->second.end()) {
                return uniform_it->second;
            }
        }

        TRIENGINE_PANIC("Subroutine uniform '%s' not found in cache for stage 0x%X."
            , uniform_name.c_str()
            , stage_enum);

        return -1; // Not found
    }

    void shader_program::set_active_subroutine(
        const shader_object_type stage_type,
        const std::string& subroutine_uniform_name, 
        const GLuint subroutine_function_index)
    {
        TRIENGINE_ASSERT(this->is_valid());
        const GLenum stage_enum = static_cast<GLenum>(stage_type);

        auto indices_map_it = _subroutine_function_indices_vector_map.find(stage_enum);
        if (indices_map_it == _subroutine_function_indices_vector_map.end()) {
            // This should not happen since its initialized in `_build_subroutine_uniform_caches`
            TRIENGINE_PANIC("Subroutine function indices vector not initialized for stage 0x%X."
                , stage_enum);
        }

        // NOTE: index == subroutine uniform location, value == subroutine function index
        std::vector<GLuint>& curr_function_indices_vec = indices_map_it->second;

        const GLint uniform_location = this->get_subroutine_uniform_location(stage_type, subroutine_uniform_name);

        // validate `uniform_location` (bounds check)
        if (uniform_location < 0 || static_cast<size_t>(uniform_location) >= curr_function_indices_vec.size()) {
            TRIENGINE_PANIC("Subroutine uniform location %d is out of bounds for stage 0x%X (indices vector size: %zu)."
                , uniform_location
                , stage_enum
                , curr_function_indices_vec.size());
        }

        // validate `subroutine_function_index`
        if (const auto find_it = _subroutine_functions_index_name_map.find(stage_enum); 
            find_it == _subroutine_functions_index_name_map.end() ||
            find_it->second.count(subroutine_function_index) == 0) {
            TRIENGINE_PANIC("Subroutine function index %u is invalid or no functions cached for stage 0x%X"
                , subroutine_function_index
                , stage_enum);
        }

        // Update uniform function indices vector
        curr_function_indices_vec[static_cast<size_t>(uniform_location)] = subroutine_function_index;
        GLCall(::glUniformSubroutinesuiv(
            stage_enum, 
            static_cast<GLsizei>(curr_function_indices_vec.size()), 
            curr_function_indices_vec.data())
        );
    }

    void shader_program::set_active_subroutine(
        const shader_object_type stage_type, 
        const std::string& subroutine_uniform_name, 
        const std::string& subroutine_function_name)
    {
        TRIENGINE_ASSERT(this->is_valid());

        const GLenum stage_enum = static_cast<GLenum>(stage_type);

        const auto stage_it = _subroutine_functions_name_index_map.find(stage_enum);
        if (stage_it == _subroutine_functions_name_index_map.end()) {
            TRIENGINE_PANIC("Cache for stage 0x%X not found or stage has no subroutine functions. (Cannot find function '%s')"
                , stage_enum
                , subroutine_function_name.c_str());
        }

        const auto function_index_it = stage_it->second.find(subroutine_function_name);
        if (function_index_it == stage_it->second.end()) {
            TRIENGINE_PANIC("Subroutine function '%s' not found in cache for stage 0x%X"
                , subroutine_function_name.c_str()
                , stage_enum);
        }

        this->set_active_subroutine(
            stage_type,
            subroutine_uniform_name,
            function_index_it->second
        );
    }

    void shader_program::_attach_shader(shader_object&& new_shader)
    {
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

    void shader_program::_build_uniforms_location_cache()
    {
        _uniforms_location_map.clear();

        // Retrieves the longest length of the uniform variable name 
        // among uniform variables (including null terminator)
        GLint max_active_uniform_name_len{};
        GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_active_uniform_name_len));

        // Get number of active uniforms
        GLint num_of_active_uniforms{};
        GLCall(::glGetProgramiv(_program_id, GL_ACTIVE_UNIFORMS, &num_of_active_uniforms));

        TRIENGINE_TRACE("----- num_of_active_uniforms = %d", num_of_active_uniforms);
        for (GLint uniform_idx{ 0 }; uniform_idx < num_of_active_uniforms; ++uniform_idx)
        {
            std::string uniform_name_buff; // variable name in GLSL
            uniform_name_buff.resize(max_active_uniform_name_len);

            GLsizei uniform_name_len{};    // name length (excluding the null terminator)
            GLint uniform_size{};          // data size of the variable
            GLenum uniform_type{};         // data type of the variable (float, vec3 or mat4, etc)

            // get the name of this uniform
            ::glGetActiveUniform(
                _program_id,
                static_cast<GLuint>(uniform_idx),
                static_cast<GLsizei>(uniform_name_buff.size()),
                &uniform_name_len,
                &uniform_size,
                &uniform_type,
                uniform_name_buff.data()
            );

            if (::glGetError() != GL_NO_ERROR) {
                TRIENGINE_PANIC("Could not get active uniform name in index %d"
                    , uniform_idx);
            }

            uniform_name_buff.resize(uniform_name_len);
            const GLint uniform_location{ ::glGetUniformLocation(_program_id, uniform_name_buff.c_str()) };
            if (uniform_location == -1) {
                TRIENGINE_PANIC("Could not get location for active uniform '%s'"
                    , uniform_name_buff.c_str());
            }

            TRIENGINE_TRACE("#%d : uniform_locations_cache[\"%s\"(%llu)] = %d"
                , uniform_idx
                , uniform_name_buff.c_str()
                , uniform_name_buff.size()
                , uniform_location);

            // cache for later use
            const auto [_, success] = _uniforms_location_map.insert(
                std::make_pair(std::move(uniform_name_buff), uniform_location)
            );

            TRIENGINE_ASSERT(success);
        }
    }

    void shader_program::_build_subroutine_uniforms_cache()
    {
        TRIENGINE_ASSERT(this->is_valid());

        _subroutine_function_indices_vector_map.clear();
        _subroutine_uniforms_name_location_map.clear();
        _subroutine_functions_name_index_map.clear();
        _subroutine_functions_index_name_map.clear();

        constexpr std::array<GLenum, 4> all_stage_enums{ 
            GL_VERTEX_SHADER, 
            GL_FRAGMENT_SHADER, 
            GL_GEOMETRY_SHADER,
            GL_COMPUTE_SHADER,
            // Add other stages if supported.. (e.g: GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER)
        };

        for (const GLenum curr_stage : all_stage_enums)
        {
            // the number of active subroutines in the stage.
            GLint num_subroutines_in_stage{ 0 };
            GLCall(::glGetProgramStageiv(_program_id, curr_stage, GL_ACTIVE_SUBROUTINES, &num_subroutines_in_stage));

            // the length of the longest subroutine name for the stage.
            // (includes space for the null-terminator)
            GLint max_subroutine_func_name_len_in_stage{ 0 };
            GLCall(::glGetProgramStageiv(_program_id, curr_stage, GL_ACTIVE_SUBROUTINE_MAX_LENGTH, &max_subroutine_func_name_len_in_stage));

            // the length of the longest subroutine uniform for the stage.
            // (includes space for the null-terminator)
            GLint max_subroutine_uniform_name_len_in_stage{ 0 };
            GLCall(::glGetProgramStageiv(_program_id, curr_stage, GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH, &max_subroutine_uniform_name_len_in_stage));

            // the number of active subroutine variables in the stage.
            GLint num_uniforms_in_stage{ 0 };
            GLCall(::glGetProgramStageiv(_program_id, curr_stage, GL_ACTIVE_SUBROUTINE_UNIFORMS, &num_uniforms_in_stage));

            // the number of active subroutine variable locations in the stage.
            // (size of function indices vector)
            GLint num_uniform_locations_in_stage{ 0 };
            GLCall(::glGetProgramStageiv(_program_id, curr_stage, GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS, &num_uniform_locations_in_stage));

            // Default to function index 0 for all uniform locations (or use `GL_INVALID_INDEX`?)
            _subroutine_function_indices_vector_map[curr_stage].assign(num_uniform_locations_in_stage, 0);

            std::vector<GLchar> tmp_uniform_name_buffer(max_subroutine_uniform_name_len_in_stage);
            for (GLint i{ 0 }; i < num_uniforms_in_stage; ++i)
            {
                GLsizei actual_uniform_name_len{ 0 };
                GLCall(::glGetActiveSubroutineUniformName(_program_id, 
                    curr_stage, 
                    i, 
                    max_subroutine_uniform_name_len_in_stage, 
                    &actual_uniform_name_len, 
                    tmp_uniform_name_buffer.data()
                ));

                const std::string uniform_name{ tmp_uniform_name_buffer.data(), static_cast<size_t>(actual_uniform_name_len) };
                const GLint uniform_location{ ::glGetSubroutineUniformLocation(_program_id, curr_stage, uniform_name.c_str()) }; // No GLCall, -1 is possible

                if (uniform_location != -1) // Should always be found if iterating active ones
                {
                    _subroutine_uniforms_name_location_map[curr_stage][uniform_name] = uniform_location;
                }
                else 
                {
                    TRIENGINE_PANIC("Could not get location for active subroutine uniform '%s' in stage 0x%X"
                        , uniform_name.c_str()
                        , curr_stage);
                }
            }

            std::vector<GLchar> tmp_func_name_buffer(max_subroutine_func_name_len_in_stage);
            for (GLint i{ 0 }; i < num_subroutines_in_stage; ++i)
            {
                GLsizei actual_func_name_len{ 0 };
                GLCall(::glGetActiveSubroutineName(_program_id, 
                    curr_stage, 
                    i, 
                    max_subroutine_func_name_len_in_stage, 
                    &actual_func_name_len, 
                    tmp_func_name_buffer.data()
                ));

                const std::string func_name{ tmp_func_name_buffer.data(), static_cast<size_t>(actual_func_name_len) };
                const GLuint func_index{ ::glGetSubroutineIndex(_program_id, curr_stage, func_name.c_str()) }; // No GLCall, GL_INVALID_INDEX possible

                if (func_index != GL_INVALID_INDEX)
                {
                    _subroutine_functions_name_index_map[curr_stage][func_name] = func_index;
                    _subroutine_functions_index_name_map[curr_stage][func_index] = func_name;
                }
                else 
                {
                    TRIENGINE_ERROR("Could not get index for active subroutine function '%s' in stage 0x%X"
                        , func_name.c_str()
                        , curr_stage);
                }
            }

        } // for
    }

} // namespace