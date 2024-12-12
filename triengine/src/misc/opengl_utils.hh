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

    // singleton
    class global_glfw_environment final {
    private:
        global_glfw_environment();

    public:
        ~global_glfw_environment();
        global_glfw_environment(const global_glfw_environment&) = delete;
        global_glfw_environment& operator=(const global_glfw_environment&) = delete;

    public:
        // This function initializes the GLFW library for the rendering. 
        // You have to run this function before creating the visualizer object.
        // NOTE: This function must be called from the main thread.
        static void initialize() {
            static global_glfw_environment inst_{};
        }

    }; // class

} // namespace