#pragma once
#include "../common.h"
#include "debug_utils.hh"

#include "../extern/glad/glad.h"
#include <GLFW/glfw3.h>

// Refs:
// https://gamesmith.tistory.com/147
// https://gamesmith.tistory.com/144
#define GLCheckError() ::triengine::misc::GLCheckError_impl(_TRIENGINE_CURRENT_SOURCE_LOC()) 
#define GLClearError() while (::glGetError() != GL_NO_ERROR);

#if defined(TRIENGINE_DEBUG_MODE)
#  define GLCall(STMT) \
    GLClearError(); \
    STMT; \
    TRIENGINE_ASSERT(GLCheckError());
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define GLCall(STMT) STMT // Call without error check
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

namespace triengine::misc
{
    bool GLCheckError_impl(
        const debug::source_loc& src_loc
    );

    // NOTE: Device screen coordinates are relative to the upper-left corner of the window content area.
    static inline vec2_f32 get_cursor_device_screen_pos(
        GLFWwindow* const glfw_window)
    {
        double xpos, ypos;
        ::glfwGetCursorPos(glfw_window, &xpos, &ypos);
        return vec2_f32{ static_cast<float>(xpos), static_cast<float>(ypos) };
    }

} // namespace