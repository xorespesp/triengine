#include "file_browser_dialog.hh"

#include <triengine/utility/string_format.hh>

#include <imgui/imgui.h>

#include <atomic>
#include <cstdint>
#include <utility>

namespace triengine::gui::widgets
{
    namespace
    {
        std::string make_unique_popup_id() {
            static std::atomic<uint32_t> counter{ 0 };
            return utility::string::c_format("###triengine-file-dialog-%08X",
                static_cast<unsigned int>(counter.fetch_add(1, std::memory_order_relaxed)));
        }

    } // namespace

    file_browser_dialog::file_browser_dialog(std::string title)
        : _popup_id{ std::move(title) + make_unique_popup_id() }
    {
        _browser.set_max_visible_items(0); // the list follows the dialog size
    }

    void file_browser_dialog::open() noexcept {
        _open_requested = true;
        _want_open = true;
        _close_requested = false;
    }

    void file_browser_dialog::close() noexcept {
        _open_requested = false;
        _want_open = false;
        _close_requested = true;
    }

    bool file_browser_dialog::show()
    {
        // OpenPopup() must run in the same ID stack as BeginPopupModal(), so open() only records the request.
        if (_open_requested) {
            ImGui::OpenPopup(_popup_id.c_str());
            const float font_size = ImGui::GetFontSize();
            ImGui::SetNextWindowSize(ImVec2(font_size * 50.0f, font_size * 32.0f), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            _open_requested = false;
        } else if (_want_open && !ImGui::IsPopupOpen(_popup_id.c_str())) {
            // ImGui closes a modal that misses a frame (e.g. the host window is minimized or a hidden dock tab);
            // reopen it where it was.
            ImGui::OpenPopup(_popup_id.c_str());
        }

        // NoSavedSettings: BeginPopupModal() does not add it, and there is nothing worth keeping in the .ini.
        bool keep_open{ true };
        if (!ImGui::BeginPopupModal(_popup_id.c_str(), &keep_open, ImGuiWindowFlags_NoSavedSettings)) {
            _want_open = _want_open && keep_open; // the title bar button closes it for good
            _close_requested = false;
            _nested_popup_open = false;
            return false;
        }

        // Esc first goes to an active item (e.g. the path editor) or a nested popup; only a later Esc closes the dialog.
        const bool item_was_active = ImGui::IsAnyItemActive();
        const bool file_selected = _browser.show();
        const bool esc_pressed = !item_was_active && !_nested_popup_open
            && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        _nested_popup_open = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

        if (file_selected || esc_pressed || _close_requested) {
            _want_open = false;
            ImGui::CloseCurrentPopup();
        }
        _close_requested = false;

        ImGui::EndPopup();
        return file_selected;
    }

} // namespace
