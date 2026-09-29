#pragma once
#include <triengine/gui/widgets/file_browser_widget.hh>

#include <string>

namespace triengine::gui::widgets
{
    // Modal dialog around `file_browser_widget`.
    // Call `show()` every frame; `open()` may be called from anywhere.
    class file_browser_dialog
    {
    public:
        explicit file_browser_dialog(std::string title = "Open File");

        file_browser_dialog(const file_browser_dialog&) = delete;
        file_browser_dialog& operator=(const file_browser_dialog&) = delete;
        file_browser_dialog(file_browser_dialog&&) = default;
        file_browser_dialog& operator=(file_browser_dialog&&) = default;

        file_browser_widget& get_browser() noexcept { return _browser; }
        const file_browser_widget& get_browser() const noexcept { return _browser; }

        void open() noexcept;
        void close() noexcept;
        bool is_open() const noexcept { return _want_open; }

        // true on the frame a file is selected (`get_browser().get_selected_path()`); the dialog then closes itself
        bool show();

    private:
        file_browser_widget _browser;
        std::string _popup_id;
        bool _open_requested{ false };
        bool _close_requested{ false };
        bool _want_open{ false }; // stays true until the user or `close()` closes the dialog
        bool _nested_popup_open{ false }; // as of the previous frame (places, hidden segments, file filter)
    };

} // namespace
