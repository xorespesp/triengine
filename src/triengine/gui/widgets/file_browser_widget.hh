#pragma once
#include <triengine/utility/enum_flags.hh>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace triengine::gui::widgets
{
    enum class file_browser_flags : uint32_t
    {
        none                   = 0,
        select_on_single_click = 1 << 0, // single click sets the selected path (default: single click highlights, double click selects)
    };
    TRIENGINE_DEFINE_ENUM_FLAG_OPERATORS(file_browser_flags)

    class file_browser_widget
    {
    private:
        class file_filter_t {
            std::string _glob_pattern; // Original glob pattern (for reference)
            std::string _glob_regex_expr; // regex string (ECMAScript syntax) for matching
            std::regex _regex; // Compiled regex for matching

        public:
            file_filter_t(std::string_view glob_pattern);

            bool matches(const std::filesystem::path& path) const;

            const std::string& get_glob_pattern() const noexcept {
                return _glob_pattern;
            }

            bool operator<(const file_filter_t& other) const noexcept {
                return _glob_regex_expr < other._glob_regex_expr;
            }
        };

        class file_item_t {
        public:
            std::string display_name; // UTF-8 file name ("..": parent link)
            std::filesystem::path full_path;
            bool is_directory{ false };
            bool is_parent_link{ false }; // ".." entry
            std::string size_text; // files only, e.g. "12.34 KB"
            std::string modified_text; // local time, e.g. "2026/09/29 14:05:09"

            file_item_t(
                std::string display_name_, 
                std::filesystem::path full_path_, 
                bool is_directory_,
                bool is_parent_link_ = false)
                : display_name{ std::move(display_name_) }
                , full_path{ std::move(full_path_) }
                , is_directory{ is_directory_ }
                , is_parent_link{ is_parent_link_ }
            { }
        };

        struct path_segment_t {
            std::string label; // UTF-8
            std::filesystem::path path;
        };

    public:
        file_browser_widget();
        file_browser_widget(const file_browser_widget&) = delete;
        file_browser_widget& operator=(const file_browser_widget&) = delete;
        file_browser_widget(file_browser_widget&&) = default;
        file_browser_widget& operator=(file_browser_widget&&) = default;

        file_browser_flags get_flags() const noexcept { return _flags; }
        void set_flags(file_browser_flags flags) noexcept { _flags = flags; }

        // get/set current browsing directory
        const std::filesystem::path& get_cwd() const noexcept { return _cwd_path; }
        void set_cwd(const std::filesystem::path& new_path);

        void set_max_visible_items(size_t num_visible_items); // 0: the list fills the remaining height

        // set file type filters. e.g: { "*.txt", "file?.txt", "file_*.txt" }
        // ("*.*" matches any file types)
        void register_file_filters(std::vector<std::string> glob_patterns);

        // set currently applied type filter
        // default value is 0 (the first type filter)
        void set_active_file_filter(size_t index);

        bool has_selected_path() const { return !_sel_path.empty(); }
        std::filesystem::path get_selected_path() const { return _sel_path; }
        void clear_selected_path();

        bool show();

    private:
        void _show_cwd_bar();
        bool _open_item(const file_item_t& item);
        bool _open_typed_path();
        void _set_highlight(const file_item_t& item);
        std::filesystem::path _resolve_user_path(std::string_view text) const;
        void _change_cwd(std::filesystem::path new_dir_path);
        bool _test_file_filter(const std::filesystem::path& file_path) const;

    private:
        std::string _widget_id;
        file_browser_flags _flags{ file_browser_flags::none };
        std::vector<char> _cwd_input_buff;
        bool _cwd_editing{ false };
        bool _cwd_edit_focus{ false };

        std::filesystem::path _cwd_path;
        std::vector<path_segment_t> _cwd_segments;
        std::filesystem::path _home_dir; // refreshed when the places popup opens
        std::vector<std::filesystem::path> _root_dirs;
        std::filesystem::path _sel_path;
        std::filesystem::path _hl_path; // highlighted item, confirmed or not
        std::filesystem::path _context_item_path; // item of the right-click menu
        std::optional<std::string> _typed_name; // UTF-8, typed by the user; otherwise the input shows the highlighted file
        std::vector<char> _file_name_buff; // ImGui buffer of the file name input
        bool _file_name_active{ false }; // the input was being edited in the previous frame

        std::vector<file_item_t> _cwd_items;
        bool _cwd_items_scroll_reset{ false }; // scroll the list back to the top after the directory changes
        size_t _max_visible_cwd_items{ 10 };

        std::vector<file_filter_t> _registered_file_filters; // Allowed file extensions for filtering, stored in lowercase
        size_t _active_file_filter_idx{ 0 };

        std::string _last_error;
    };

} // namespace
