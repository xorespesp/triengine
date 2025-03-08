#include "shader.hh"

namespace triengine
{
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