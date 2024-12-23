#include "opengl_utils.hh"

#include <GLFW/glfw3.h>
#include <iostream>

bool triengine::misc::GLCheckError_impl(const debug::source_loc& src_loc)
{
    std::string err_msg;

    // Note, that glGetError() may return multiple error enumerators, 
    // so you'd need to call this function in a loop until it returns GL_NO_ERROR or the errors will remain.
    while (GLenum gl_err = ::glGetError()) {
        switch (gl_err) {
        case GL_INVALID_ENUM:                  err_msg += " INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 err_msg += " INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             err_msg += " INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW:                err_msg += " STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW:               err_msg += " STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY:                 err_msg += " OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: err_msg += " INVALID_FRAMEBUFFER_OPERATION"; break;
        }
    }

    if (!err_msg.empty()) {
        const auto src_filename = src_loc.filename();
        const auto msg = misc::string::c_format(""
            "OpenGL error at <%.*s:#L%d(%.*s)> :%s"
            , static_cast<int>(src_filename.size())
            , src_filename.data()
            , src_loc.line
            , static_cast<int>(src_loc.funcname.size())
            , src_loc.funcname.data()
            , err_msg.c_str()
        );
        std::cout << '\n' << msg << std::endl;
        return false;
    }

    return true;
}

triengine::misc::global_glfw_environment::global_glfw_environment()
{
    if (!::glfwInit()) {
        std::cout << "\nglfwInit() failed" << std::endl;
        ::exit(EXIT_FAILURE);
    }

    ::glfwSetErrorCallback(+[](const int err_code, const char* const err_desc) -> void {
        const auto msg = misc::string::c_format(""
            "GLFW Error(%d) : %s"
            , err_code
            , err_desc
        );
        std::cout << '\n' << msg << std::endl;
    });
}

triengine::misc::global_glfw_environment::~global_glfw_environment()
{
    ::glfwTerminate();
}