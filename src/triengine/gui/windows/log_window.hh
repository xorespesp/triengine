#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/utility/logger.hh>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace triengine::gui
{
    // Scrolling console for engine & application logs.
    class log_window
        : public iwindow
    {
    public:
        // Number of records the window keeps by default; the oldest is dropped past it.
        static constexpr size_t kDefaultLogCapacity{ 10000 };

        explicit log_window(
            bool capture_engine_logs = true,
            size_t capacity = kDefaultLogCapacity
        );

        virtual ~log_window();

        const char* get_window_name() const override {
            return "Logs";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 450.0f, 300.0f };
        }

        // (gui thread)
        void render(const window_render_context& render_ctx) override;

        // (thread-safe)
        void add_log(utility::log_level lv, std::string_view sv);

        // (thread-safe)
        void add_log(std::string_view sv) {
            this->add_log(utility::log_level::info, sv);
        }

        // (thread-safe)
        template <typename... _Args>
        void add_logf(utility::log_level lv, const std::string& c_fmt, _Args&&... args) {
            this->add_log(lv, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
        }

        // (thread-safe)
        template <typename... _Args>
        void add_logf(const std::string& c_fmt, _Args&&... args) {
            this->add_log(utility::log_level::info, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
        }

    private:
        struct impl_t;
        std::unique_ptr<impl_t> _impl;

    }; // class

} // namespace
