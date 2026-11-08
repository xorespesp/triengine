#pragma once
#include <triengine/common.h>
#include <triengine/utility/debug_utils.hh>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------------
// GL error checks
// ---------------------------------------------------------------------------------

// Refs:
// https://gamesmith.tistory.com/147
// https://gamesmith.tistory.com/144
#define GLCheckError() ::triengine::utility::GLCheckError_impl(_TRIENGINE_CURRENT_SOURCE_LOC())
#define GLClearError() while (::glGetError() != GL_NO_ERROR);

namespace triengine::utility
{
    bool GLCheckError_impl(
        const source_loc& src_loc
    );
}

// ---------------------------------------------------------------------------------
// GL call counting (debug mode only)
// Every `GLCall()` site registers itself on its first call, then counts its calls per thread.
// Call counts do not depend on the build, so a debug-mode build can count while a release one times.
// ---------------------------------------------------------------------------------

#if defined(TRIENGINE_DEBUG_MODE)
namespace triengine::utility
{
    // Index of a registered `GLCall()` site, in registration order.
    using gl_call_site_id_t = size_t;

    struct gl_call_site_info {
        std::string_view function_name;
        source_loc location;
    };

    // Registered `GLCall()` sites. IDs are dense and stable for the process.
    gl_call_site_id_t register_gl_call_site(std::string_view function_name, const source_loc& location);
    size_t gl_call_site_count() noexcept;
    gl_call_site_info get_gl_call_site(gl_call_site_id_t site_id);

    // `GLCall()` calls the calling thread made between `begin()` and `end()`, per site.
    class gl_call_stats {
    public:
        void begin();
        void end();

        // Sites registered by the last `end()`.
        size_t site_count() const noexcept { return _site_calls.size(); }
        uint64_t site_calls(gl_call_site_id_t site_id) const noexcept;

        uint64_t total_calls() const noexcept;

        // Calls to GL functions whose name starts with `function_prefix`, e.g. "glProgramUniform".
        uint64_t calls_with_prefix(std::string_view function_prefix) const;

    private:
        std::vector<uint64_t> _begin_counts;
        std::vector<uint64_t> _site_calls;
    };

    // Used by the `GLCall()` macro only
    namespace detail
    {
        constexpr bool is_identifier_char(const char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        }

        // "::glXxx(...)" -> "glXxx"; empty when the statement is not such a call.
        constexpr std::string_view gl_function_name(std::string_view stmt) noexcept
        {
            if (stmt.substr(0, 2) == "::") {
                stmt.remove_prefix(2);
            }

            size_t name_length = 0;
            while (name_length < stmt.size() && is_identifier_char(stmt[name_length])) {
                ++name_length;
            }
            const std::string_view name = stmt.substr(0, name_length);

            std::string_view rest = stmt.substr(name_length);
            while (!rest.empty() && rest.front() == ' ') {
                rest.remove_prefix(1);
            }

            const bool is_gl_call = name.size() > 2 && name.substr(0, 2) == "gl" && !rest.empty() && rest.front() == '(';
            return is_gl_call ? name : std::string_view{};
        }

        inline constexpr size_t kMaxGlCallSites = 2048;

        // Calls per site made by this thread. GL calls only happen on threads with a current context.
        extern thread_local uint64_t t_gl_call_counts[kMaxGlCallSites];

        inline void count_gl_call(const gl_call_site_id_t site_id) noexcept {
            ++t_gl_call_counts[site_id];
        }
    }
}
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

// ---------------------------------------------------------------------------------
// GLCall()
// ---------------------------------------------------------------------------------

#if defined(TRIENGINE_DEBUG_MODE)
#  define _TRIENGINE_GLCALL_IMPL(STMT) GLClearError(); STMT; TRIENGINE_ASSERT(GLCheckError())
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define _TRIENGINE_GLCALL_IMPL(STMT) STMT // Call without error check
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

// The GL function name is taken from the statement text at compile time.
// `GLCall()` stringifies `STMT` itself: by the time an inner macro sees it, glad has already rewritten `glXxx` to `glad_glXxx`.
#if defined(TRIENGINE_DEBUG_MODE)
#  define _TRIENGINE_GLCALL_COUNT(STMT_TEXT) \
    static constexpr std::string_view _gl_func_name_ = ::triengine::utility::detail::gl_function_name(STMT_TEXT); \
    static_assert(!_gl_func_name_.empty(), "GLCall() expects a `::glXxx(...)` call"); \
    static const ::triengine::utility::gl_call_site_id_t _call_site_ = ::triengine::utility::register_gl_call_site(_gl_func_name_, _TRIENGINE_CURRENT_SOURCE_LOC()); \
    ::triengine::utility::detail::count_gl_call(_call_site_);
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define _TRIENGINE_GLCALL_COUNT(STMT_TEXT)
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

#define GLCall(STMT) do { _TRIENGINE_GLCALL_COUNT(#STMT) _TRIENGINE_GLCALL_IMPL(STMT); } while (0)
