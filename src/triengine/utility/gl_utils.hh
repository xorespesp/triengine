#pragma once
#include <triengine/common.h>
#include <triengine/utility/debug_utils.hh>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

// Refs:
// https://gamesmith.tistory.com/147
// https://gamesmith.tistory.com/144
#define GLCheckError() ::triengine::utility::GLCheckError_impl(_TRIENGINE_CURRENT_SOURCE_LOC()) 
#define GLClearError() while (::glGetError() != GL_NO_ERROR);

#if defined(TRIENGINE_DEBUG_MODE)
#  define GLCall(STMT) \
    GLClearError(); \
    STMT; \
    TRIENGINE_ASSERT(GLCheckError());
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define GLCall(STMT) STMT // Call without error check
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

namespace triengine::utility
{
    bool GLCheckError_impl(
        const source_loc& src_loc
    );

} // namespace