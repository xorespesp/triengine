#pragma once
#include <triengine/common.h>
#include <triengine/utility/string_format.hh>
#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <set>
#include <regex>
#include <optional>
#include <imgui/imgui.h>

namespace triengine::gui::widgets
{
    class file_browser_widget
    {
        class file_filter_t
        {
            std::string _glob_pattern; // Original glob pattern (for reference)
            std::string _glob_regex_expr; // regex string (ECMAScript syntax) for matching
            std::regex _regex; // Compiled regex for matching

        public:
            file_filter_t(std::string_view glob_pattern)
                : _glob_pattern{ glob_pattern }
                , _glob_regex_expr{ glob_to_regex_string(glob_pattern) }
                , _regex{ _glob_regex_expr, std::regex::icase/* case-insensitive matching */ }
            { }

            bool matches(const std::filesystem::path& path) const {
                if (!path.has_filename()) { return false; }
                try {
                    return std::regex_match(path.filename().u8string(), _regex);
                } catch (const std::regex_error&) {
                    return false;
                }
            }

            const std::string& get_glob_pattern() const noexcept {
                return _glob_pattern;
            }

            bool operator<(const file_filter_t& other) const noexcept {
                return _glob_regex_expr < other._glob_regex_expr;
            }
        };

        class file_item_t {
        public:
            std::string display_name;
            std::filesystem::path full_path;
            bool is_directory{ false };

            file_item_t(
                std::string display_name_, 
                std::filesystem::path full_path_, 
                bool is_directory_)
                : display_name{ std::move(display_name_) }
                , full_path{ std::move(full_path_) }
                , is_directory{ is_directory_ }
            { }
        };

        // Helper to convert std::string to lowercase (from previous version)
        static std::string to_lower_str(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        // Helper function to convert a glob-like pattern to an ECMAScript regex string
        static std::string glob_to_regex_string(const std::string_view glob_pattern) {
            std::string regex_str = "^"; // Anchor to the beginning of the string
            for (char c : glob_pattern) {
                switch (c) {
                case '*':
                    regex_str += ".*"; // glob '*' corresponds to regex '.*' (zero or more of any character)
                    break;
                case '?':
                    regex_str += ".";  // glob '?' corresponds to regex '.' (any single character)
                    break;
                // Special characters in ECMAScript regex that need to be escaped
                // if they are to be treated as literal characters from the glob pattern.
                case '.': case '\\': case '+': case '(': case ')':
                case '[': case ']':  case '{': case '}': case '^':
                case '$': case '|':
                    regex_str += '\\'; // Add escape character
                    regex_str += c;
                    break;
                default:
                    regex_str += c;    // Add literal characters as they are
                    break;
                }
            }
            regex_str += "$"; // Anchor to the end of the string
            return regex_str;
        }

    public:
        file_browser_widget(
            const char* label = "file-browser")
            : _widget_id{ label }
        {
            _cwd_input_buff.resize(4096, 0);

            try {
                this->_change_cwd(std::filesystem::current_path());
            }
            catch (const std::filesystem::filesystem_error& e) {
                _last_error = "Error initializing CWD: " + std::string(e.what());

                std::filesystem::path fallback_path = "/";

#ifdef _TRIENGINE_PLATFORM_WIN32
                fallback_path = "C:\\";
                if (!std::filesystem::exists(fallback_path) || !std::filesystem::is_directory(fallback_path)) {
                    // Try to find a valid drive or default to current directory if C:\ is not suitable
                    char drv_str[] = "A:\\";
                    for (char drv_letter = 'A'; drv_letter <= 'Z'; ++drv_letter) {
                        drv_str[0] = drv_letter;
                        if (std::filesystem::exists(drv_str) && std::filesystem::is_directory(drv_str)) {
                            fallback_path = drv_str;
                            break;
                        }
                    }
                    if (fallback_path == "C:\\" && (!std::filesystem::exists(fallback_path) || !std::filesystem::is_directory(fallback_path))) {
                        fallback_path = "."; // Last resort
                    }
                }
#endif
                this->_change_cwd(fallback_path);
            }
        }

        // get current browsing directory
        const std::filesystem::path& get_cwd() const noexcept {
            return _cwd_path;
        }

        // set current browsing directory
        void set_cwd(const std::filesystem::path& new_path) {
            this->_change_cwd(new_path);
        }

        void set_max_visible_items(size_t num_visible_items) {
            if (!num_visible_items) {
                throw std::invalid_argument{ "invalid max visible cwd items" };
            }
            _max_visible_cwd_items = num_visible_items;
        }

        // set file type filters. e.g: { "*.txt", "file?.txt", "file_*.txt" }
        // ("*.*" matches any file types)
        void register_file_filters(std::vector<std::string> glob_patterns) {
            _registered_file_filters.clear();
            for (const auto& glob_pattern : glob_patterns) {
                _registered_file_filters.emplace_back(glob_pattern);
            }
            this->set_active_file_filter(0);
        }

        // set currently applied type filter
        // default value is 0 (the first type filter)
        void set_active_file_filter(size_t index) {
            if (index >= _registered_file_filters.size()) {
                throw std::invalid_argument{ "invalid filter index" };
            }
            _active_file_filter_idx = index;
            if (!this->_test_file_filter(_sel_path)) {
                _sel_path.clear();
            }
            this->_change_cwd(_cwd_path); // invalidate cwd items
        }

        bool has_selected_path() const {
            return !_sel_path.empty();
        }

        std::filesystem::path get_selected_path() const {
            return _sel_path;
        }

        void clear_selected_path() {
            _sel_path.clear();
        }

        bool show()
        {
            struct ImGuiScopedID final {
                ImGuiScopedID(const std::string& str_id) { ImGui::PushID(str_id.c_str()); }
                ~ImGuiScopedID() { ImGui::PopID(); }
            } id_scope{ _widget_id };

            bool file_was_selected_this_frame{ false };

            ImGui::Text("Current: ");
            ImGui::SameLine();
            if (ImGui::InputText("##cwd-input", 
                _cwd_input_buff.data(), 
                _cwd_input_buff.size(), 
                ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                // _change_cwd modifies _cwd_items, so handle it before the loop
                this->_change_cwd(std::filesystem::path(_cwd_input_buff.data()));
            }

            if (this->has_selected_path()) {
                ImGui::Text("Selected: ");
                ImGui::SameLine();
                ImGui::InputText("##sel-input",
                    this->get_selected_path().string().data(),
                    this->get_selected_path().string().size(),
                    ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_ReadOnly
                );
            }

            if (!_registered_file_filters.empty()) {
                ImGui::Text("File Filter: ");
                ImGui::SameLine();
                if (ImGui::BeginCombo("##file-filter-dropdown",
                    _registered_file_filters[_active_file_filter_idx].get_glob_pattern().c_str(),
                    ImGuiComboFlags_WidthFitPreview))
                {
                    for (size_t n = 0; n < _registered_file_filters.size(); ++n)
                    {
                        const bool is_selected = n == _active_file_filter_idx;
                        if (ImGui::Selectable(_registered_file_filters[n].get_glob_pattern().c_str(), is_selected)) {
                            _active_file_filter_idx = n;
                            if (!this->_test_file_filter(_sel_path)) {
                                _sel_path.clear();
                            }
                            this->_change_cwd(_cwd_path); // invalidate cwd items
                        }

                        // Focus on the default selected item when the dropdown opens.
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (!_last_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", _last_error.c_str());
            }

            std::optional<int32_t> sel_item_idx;
            const float list_box_height{ ImGui::GetTextLineHeightWithSpacing() * _max_visible_cwd_items };
            if (ImGui::BeginListBox("##cwd-items", ImVec2(-FLT_MIN, list_box_height))) {
                for (int32_t i = 0; i < static_cast<int32_t>(_cwd_items.size()); ++i) {
                    ImGui::PushStyleColor(ImGuiCol_Text
                        , _cwd_items[i].is_directory
                        ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                        : ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
                    );

                    if (ImGui::Selectable(_cwd_items[i].display_name.c_str(), false)) {
                        sel_item_idx = i;
                    }

                    ImGui::PopStyleColor();
                }

                ImGui::EndListBox();
            }

            if (sel_item_idx.value_or(_cwd_items.size()) < _cwd_items.size())
            {
                const auto& sel_item = _cwd_items[sel_item_idx.value()];
                if (sel_item.is_directory) {
                    this->_change_cwd(sel_item.full_path);
                } else { // is file
                    _sel_path = sel_item.full_path;
                    file_was_selected_this_frame = true;
                }
            }

            return file_was_selected_this_frame;
        }

    private:
        void _change_cwd(std::filesystem::path new_dir_path)
        {
            _last_error.clear();

            if (new_dir_path.empty()) {
                _last_error = "Path cannot be empty.";
                return;
            }

            // try to resolve symlinks and normalize the path
            std::error_code ec;
            std::filesystem::path canonical_new_path = std::filesystem::weakly_canonical(new_dir_path, ec);
            if (!ec) {
                new_dir_path = canonical_new_path;
            } else {
                _last_error = "Path normalization failed: " + ec.message();
            }

            // Clear the current working directory items
            _cwd_items.clear();

            // The entered directory path is stored in the input buffer regardless of whether the path is actually valid.
            std::snprintf(_cwd_input_buff.data(), _cwd_input_buff.size(), "%s", new_dir_path.string().c_str());

            // Validate the new directory path
            if (!std::filesystem::is_directory(new_dir_path)) {
                _last_error = "Invalid directory path: " + new_dir_path.string();
                // If the directory path is invalid, skip the cwd items update.
                return;
            }

            // Update the current working directory path
            _cwd_path = new_dir_path;

            // Update the current working directory items ...
            // add ".." item to cwd items (if the current path has a parent)
            if (_cwd_path.has_parent_path()) {
                std::filesystem::path parent_p = _cwd_path.parent_path();
                // Prevent moving to parent of root directory (e.g., C:\ to C:\)
                if (parent_p != _cwd_path) {
                    // Add ".." only if the parent path is different from the current path
                    try {
                        // Check actual path using canonical (considering symlinks, etc.)
                        std::filesystem::path canonical_parent = std::filesystem::canonical(parent_p);
                        std::filesystem::path canonical_current = std::filesystem::canonical(_cwd_path);
                        if (canonical_parent != canonical_current) {
                            _cwd_items.emplace_back("[D] ..", canonical_parent, true);
                        }
                    }
                    catch (const std::filesystem::filesystem_error& e) {
                        _last_error = "Could not add '..' entry: " + std::string{ e.what() };
                    }
                }
            }

            std::vector<file_item_t> temp_dir_items, temp_file_items;

            try
            {
                for (const auto& dir_entry : std::filesystem::directory_iterator(_cwd_path))
                {
                    std::string entry_filename_str = dir_entry.path().filename().string();
                    if (entry_filename_str.empty() || entry_filename_str == "." || entry_filename_str == "..")
                    {
                        continue;
                    }

                    if (dir_entry.is_directory())
                    {
                        temp_dir_items.emplace_back("[D] " + entry_filename_str, dir_entry.path(), true);
                    }
                    else if (dir_entry.is_regular_file())
                    {
                        if (this->_test_file_filter(dir_entry.path())) {
                            temp_file_items.emplace_back("[F] " + entry_filename_str, dir_entry.path(), false);
                        }
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                _last_error = "Error accessing directory contents: " + std::string(e.what());
                // _browser_items may already be cleared or partially filled.
                // ensure it's empty on error to avoid showing stale data.
                _cwd_items.clear();
                if (_cwd_path.has_parent_path() && _cwd_path.parent_path() != _cwd_path) { // Re-add ".." if possible
                    _cwd_items.emplace_back("[D] ..", _cwd_path.parent_path(), true);
                }
                return;
            }

            // sort directories and files separately, by actual name
            auto sort_rule = [](const file_item_t& a, const file_item_t& b) {
                return to_lower_str(a.full_path.filename().string()) < to_lower_str(b.full_path.filename().string());
            };
            std::sort(temp_dir_items.begin(), temp_dir_items.end(), sort_rule);
            std::sort(temp_file_items.begin(), temp_file_items.end(), sort_rule);

            _cwd_items.insert(_cwd_items.end(), temp_dir_items.begin(), temp_dir_items.end());
            _cwd_items.insert(_cwd_items.end(), temp_file_items.begin(), temp_file_items.end());
        }

        bool _test_file_filter(const std::filesystem::path& file_path) const
        {
            if (_registered_file_filters.empty()) {
                return true; // No filters, show all files
            }

            const auto& active_filter = _registered_file_filters.at(_active_file_filter_idx);
            return active_filter.matches(file_path);
        }

    private:
        std::string _widget_id;
        std::vector<char> _cwd_input_buff;

        std::filesystem::path _cwd_path;
        std::filesystem::path _sel_path;

        std::vector<file_item_t> _cwd_items;
        size_t _max_visible_cwd_items{ 10 };

        std::vector<file_filter_t> _registered_file_filters; // Allowed file extensions for filtering, stored in lowercase
        size_t _active_file_filter_idx{ 0 };

        std::string _last_error;
    };

} // namespace
