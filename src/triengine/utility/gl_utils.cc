#include "gl_utils.hh"

#include <GLFW/glfw3.h>
#include <atomic>
#include <iostream>
#include <mutex>

// ---------------------------------------------------------------------------------
// GL error checks
// ---------------------------------------------------------------------------------

bool triengine::utility::GLCheckError_impl(const source_loc& src_loc)
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
        const auto msg = utility::string::c_format(""
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

// ---------------------------------------------------------------------------------
// GL call counting
// ---------------------------------------------------------------------------------

#if defined(TRIENGINE_DEBUG_MODE)
namespace triengine::utility
{
    namespace detail
    {
        thread_local uint64_t t_gl_call_counts[kMaxGlCallSites]{};
    }

    namespace
    {
        std::mutex g_sites_mutex;
        std::atomic<size_t> g_site_count{ 0 };

        // Function-local, so a site registered during static initialization finds it constructed.
        std::vector<gl_call_site_info>& sites() {
            static std::vector<gl_call_site_info> s_sites;
            return s_sites;
        }
    }

    gl_call_site_id_t register_gl_call_site(const std::string_view function_name, const source_loc& location)
    {
        std::scoped_lock lock{ g_sites_mutex };
        auto& registered = sites();
        if (registered.size() >= detail::kMaxGlCallSites) {
            TRIENGINE_PANIC("Too many GLCall() sites; raise detail::kMaxGlCallSites");
        }
        registered.push_back(gl_call_site_info{ function_name, location });
        g_site_count.store(registered.size(), std::memory_order_release);
        return registered.size() - 1;
    }

    size_t gl_call_site_count() noexcept
    {
        return g_site_count.load(std::memory_order_acquire);
    }

    gl_call_site_info get_gl_call_site(const gl_call_site_id_t site_id)
    {
        std::scoped_lock lock{ g_sites_mutex };
        if (site_id >= sites().size()) { TRIENGINE_PANIC("Invalid GLCall() site ID"); }
        return sites()[site_id];
    }

    void gl_call_stats::begin()
    {
        const size_t count = gl_call_site_count();
        _begin_counts.assign(detail::t_gl_call_counts, detail::t_gl_call_counts + count);
    }

    void gl_call_stats::end()
    {
        // Sites registered after `begin()` started from zero on this thread.
        const size_t count = gl_call_site_count();
        _site_calls.resize(count);
        for (gl_call_site_id_t id = 0; id < count; ++id) {
            const uint64_t at_begin = id < _begin_counts.size() ? _begin_counts[id] : 0;
            _site_calls[id] = detail::t_gl_call_counts[id] - at_begin;
        }
    }

    uint64_t gl_call_stats::site_calls(const gl_call_site_id_t site_id) const noexcept
    {
        return site_id < _site_calls.size() ? _site_calls[site_id] : 0;
    }

    uint64_t gl_call_stats::total_calls() const noexcept
    {
        uint64_t total = 0;
        for (const uint64_t calls : _site_calls) {
            total += calls;
        }
        return total;
    }

    uint64_t gl_call_stats::calls_with_prefix(const std::string_view function_prefix) const
    {
        std::scoped_lock lock{ g_sites_mutex };
        const auto& registered = sites();

        uint64_t total = 0;
        for (gl_call_site_id_t id = 0; id < _site_calls.size(); ++id) {
            if (registered[id].function_name.substr(0, function_prefix.size()) == function_prefix) {
                total += _site_calls[id];
            }
        }
        return total;
    }
}
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^
