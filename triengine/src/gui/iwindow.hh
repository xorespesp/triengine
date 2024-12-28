#pragma once
#include "../extern/imgui/imgui.h"
#include "../extern/imgui/imgui_internal.h"
#include "../extern/imgui/imgui_impl_glfw.h"
#include "../extern/imgui/imgui_impl_opengl3.h"
#include "../extern/implot/implot.h"

#include <functional>

namespace triengine::gui
{
    struct window_render_context {
        float dpi_scale{ 1.0f };

        window_render_context() = default;
    };

    class iwindow
    {
    private:
        bool _flag_show_window = true;
        std::function<void(bool)> _cb_on_visible_changed;

    public:
        iwindow() = default;
        virtual ~iwindow() = default;

        iwindow(const iwindow&) = delete;
        iwindow& operator=(const iwindow&) = delete;
        iwindow(const iwindow&&) = delete;
        iwindow& operator=(const iwindow&&) = delete;

        bool is_visible() const noexcept { return _flag_show_window; }
        void set_visible(const bool visible) noexcept { 
            const bool update = _flag_show_window != visible;
            _flag_show_window = visible;
            if (update && _cb_on_visible_changed) {
                _cb_on_visible_changed(visible);
            }
        }

        void set_on_visible_changed(decltype(_cb_on_visible_changed) cb) {
            _cb_on_visible_changed = std::move(cb);
        }

        virtual const char* get_window_name() const = 0;
        virtual ImVec2 get_initial_window_size() const = 0;

        // Draw widgets to fill your window (ImGui::Begin()/ImGui::End() will be called for you).
        virtual void render(const window_render_context& /*render_ctx*/) = 0;
        virtual void pre_render(ImGuiWindowFlags& /*window_flags*/) {}
        virtual void post_render() {}
    };

} // namespace