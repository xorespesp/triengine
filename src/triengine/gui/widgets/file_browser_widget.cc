#include "file_browser_widget.hh"

#include <triengine/common.h>
#include <triengine/utility/string_format.hh>
#include <triengine/extern/fonts/IconsFontAwesome5.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <system_error>

#if defined(_TRIENGINE_PLATFORM_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shlobj.h> // SHGetKnownFolderPath()
#else
#  include <pwd.h>
#  include <unistd.h>
#endif

namespace triengine::gui::widgets
{
    namespace
    {
        // For display and matching; never throws.
        std::string path_to_utf8(const std::filesystem::path& path) {
#if defined(_TRIENGINE_PLATFORM_WIN32)
            // path::u8string() throws on a name with an unpaired UTF-16 surrogate, which NTFS allows;
            // WideCharToMultiByte() substitutes U+FFFD instead.
            const std::wstring& wide = path.native();
            if (wide.empty()) {
                return {};
            }
            const int wide_len = static_cast<int>(wide.size());
            const int utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, nullptr, 0, nullptr, nullptr);
            std::string utf8(static_cast<size_t>(utf8_len), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, utf8.data(), utf8_len, nullptr, nullptr);
            return utf8;
#elif defined(__cpp_char8_t)
            const std::u8string s = path.u8string();
            return std::string(s.begin(), s.end());
#else
            return path.u8string();
#endif
        }

        std::filesystem::path utf8_to_path(const std::string_view s) {
#if defined(__cpp_char8_t)
            return std::filesystem::path(std::u8string(s.begin(), s.end()));
#else
            return std::filesystem::u8path(s.begin(), s.end());
#endif
        }

        // Pasted paths often carry surrounding blanks or quotes (e.g. Windows "Copy as path").
        std::string_view trim_user_path(std::string_view text) {
            auto is_blank = [](const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
            while (!text.empty() && is_blank(text.front())) { text.remove_prefix(1); }
            while (!text.empty() && is_blank(text.back())) { text.remove_suffix(1); }
            if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
                text = text.substr(1, text.size() - 2);
            }
            return text;
        }

        // Natural order: digit runs by value ("2" < "10"), letters case-insensitively; ties by leading zeros, then bytes.
        bool natural_less(const std::string_view a, const std::string_view b) {
            auto is_digit = [](const char c) { return c >= '0' && c <= '9'; };
            int tiebreak = 0; // first leading-zeros difference: < 0 puts `a` first
            size_t i = 0, j = 0;
            while (i < a.size() && j < b.size()) {
                if (is_digit(a[i]) && is_digit(b[j])) {
                    const size_t a_zeros_begin = i, b_zeros_begin = j;
                    while (i < a.size() && a[i] == '0') { ++i; }
                    while (j < b.size() && b[j] == '0') { ++j; }
                    const size_t a_num_begin = i, b_num_begin = j;
                    while (i < a.size() && is_digit(a[i])) { ++i; }
                    while (j < b.size() && is_digit(b[j])) { ++j; }

                    // Compare by digit count first, so numbers of any length never overflow.
                    const size_t a_num_len = i - a_num_begin, b_num_len = j - b_num_begin;
                    if (a_num_len != b_num_len) {
                        return a_num_len < b_num_len;
                    }
                    if (const int c = a.compare(a_num_begin, a_num_len, b, b_num_begin, b_num_len); c != 0) {
                        return c < 0;
                    }
                    const size_t a_zeros = a_num_begin - a_zeros_begin, b_zeros = b_num_begin - b_zeros_begin;
                    if (tiebreak == 0 && a_zeros != b_zeros) {
                        tiebreak = a_zeros < b_zeros ? -1 : 1;
                    }
                    continue;
                }

                const int x = std::tolower(static_cast<unsigned char>(a[i]));
                const int y = std::tolower(static_cast<unsigned char>(b[j]));
                if (x != y) {
                    return x < y;
                }
                ++i;
                ++j;
            }

            const size_t a_rest = a.size() - i, b_rest = b.size() - j;
            if (a_rest != b_rest) {
                return a_rest < b_rest;
            }
            if (tiebreak != 0) {
                return tiebreak < 0;
            }
            return a < b;
        }

        // Helper function to convert a glob-like pattern to an ECMAScript regex string
        std::string glob_to_regex_string(const std::string_view glob_pattern) {
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

        // 1024-based, e.g. "512 B", "12.34 KB", "1.50 GB"
        std::string format_file_size(const uintmax_t num_bytes) {
            if (num_bytes < 1024) {
                return std::to_string(num_bytes) + " B";
            }
            constexpr const char* kUnits[] = { "KB", "MB", "GB" };
            double value = static_cast<double>(num_bytes) / 1024.0;
            size_t unit = 0;
            // 1023.995 would print as "1024.00", so move to the next unit first
            while (value >= 1023.995 && unit + 1 < std::size(kUnits)) {
                value /= 1024.0;
                ++unit;
            }
            char buff[32];
            std::snprintf(buff, sizeof(buff), "%.2f %s", value, kUnits[unit]);
            return buff;
        }

        // Local time as "YYYY/MM/DD HH:MM:SS"
        std::string format_file_time(const std::filesystem::file_time_type file_time) {
            // C++17 has no clock_cast; shift by the offset between the two clocks' "now" instead.
            const auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            const std::time_t time = std::chrono::system_clock::to_time_t(sys_time);

            std::tm local_tm{};
#if defined(_TRIENGINE_PLATFORM_WIN32)
            if (::localtime_s(&local_tm, &time) != 0) { return {}; }
#else
            if (!::localtime_r(&time, &local_tm)) { return {}; }
#endif
            char buff[32];
            return std::strftime(buff, sizeof(buff), "%Y/%m/%d %H:%M:%S", &local_tm) > 0 ? buff : std::string{};
        }

        std::string make_unique_widget_id() {
            static std::atomic<uint32_t> counter{ 0 };
            return utility::string::c_format(
                "##triengine-file-browser-%08X"
                , counter.fetch_add(1, std::memory_order_relaxed)
            );
        }

        float calc_button_width(const char* label) {
            return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }

        // FontAwesome glyphs that `gui_manager` merges into the GUI fonts; they render as '?' with other fonts.
        const char* const kIconPlaces = reinterpret_cast<const char*>(ICON_FA_HDD);
        const char* const kIconHome = reinterpret_cast<const char*>(ICON_FA_HOME);
        const char* const kIconEditPath = reinterpret_cast<const char*>(ICON_FA_PEN);
        const char* const kIconFolder = reinterpret_cast<const char*>(ICON_FA_FOLDER);
        const char* const kIconFile = reinterpret_cast<const char*>(ICON_FA_FILE);
        const char* const kIconParentFolder = reinterpret_cast<const char*>(ICON_FA_LEVEL_UP_ALT);

        constexpr ImVec4 kErrorTextColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        constexpr ImVec4 kTransparentColor{ 0.0f, 0.0f, 0.0f, 0.0f };
        constexpr ImVec4 kFolderIconColor{ 0.95f, 0.82f, 0.45f, 1.0f }; // the usual folder yellow, only for the glyph

        // Table colors derived from the theme: stripe < header, and stripe < highlight on any background.
        constexpr float kHeaderAlphaScale = 1.5f; // header: the column separator color, a bit stronger
        constexpr float kStripeAlphaScale = 0.25f; // stripe: the highlight color at a fraction of its strength

        ImVec4 scale_alpha(ImVec4 color, const float scale) {
            color.w = std::min(1.0f, color.w * scale);
            return color;
        }

        std::filesystem::path find_home_dir() {
#if defined(_TRIENGINE_PLATFORM_WIN32)
            // Wide API: the narrow USERPROFILE variable is in the ANSI code page and breaks non-ASCII user names.
            PWSTR wide_path = nullptr;
            std::filesystem::path home;
            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &wide_path))) {
                home = wide_path;
            }
            ::CoTaskMemFree(wide_path);
            return home;
#else
            if (const char* env = std::getenv("HOME"); env && *env) {
                return env;
            }
            if (const passwd* pw = ::getpwuid(::getuid()); pw && pw->pw_dir) {
                return pw->pw_dir;
            }
            return {};
#endif
        }

        // Drives on Windows; the root and the usual mount locations elsewhere
        std::vector<std::filesystem::path> list_root_dirs() {
            std::vector<std::filesystem::path> roots;
#if defined(_TRIENGINE_PLATFORM_WIN32)
            // GetLogicalDrives() does not access the drives, so a disconnected network drive cannot stall it.
            const DWORD drive_mask = ::GetLogicalDrives();
            for (int i = 0; i < 26; ++i) {
                if (drive_mask & (DWORD{ 1 } << i)) {
                    const wchar_t drive[] = { static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0' };
                    roots.emplace_back(drive);
                }
            }
#else
            roots.emplace_back("/");

            std::vector<std::filesystem::path> mount_parents;
#  if defined(__APPLE__)
            mount_parents.emplace_back("/Volumes");
#  else
            const char* user = std::getenv("USER");
            if (!user || !*user) {
                const passwd* pw = ::getpwuid(::getuid());
                user = pw ? pw->pw_name : nullptr;
            }
            if (user && *user) {
                mount_parents.push_back(std::filesystem::path{ "/media" } / user);
                mount_parents.push_back(std::filesystem::path{ "/run/media" } / user);
            }
            mount_parents.emplace_back("/mnt");
#  endif
            for (const auto& parent : mount_parents) {
                std::error_code ec;
                for (std::filesystem::directory_iterator it{ parent, ec }, end; !ec && it != end; it.increment(ec)) {
                    if (it->is_directory(ec)) {
                        roots.push_back(it->path());
                    }
                }
            }
#endif
            return roots;
        }

    } // namespace

    file_browser_widget::file_filter_t::file_filter_t(std::string_view glob_pattern)
        : _glob_pattern{ glob_pattern }
        , _glob_regex_expr{ glob_to_regex_string(glob_pattern) }
        , _regex{ _glob_regex_expr, std::regex::icase/* case-insensitive matching */ }
    { }

    bool file_browser_widget::file_filter_t::matches(const std::filesystem::path& path) const {
        if (!path.has_filename()) { return false; }
        try {
            return std::regex_match(path_to_utf8(path.filename()), _regex);
        } catch (const std::regex_error&) {
            return false;
        }
    }

    file_browser_widget::file_browser_widget()
        : _widget_id{ make_unique_widget_id() }
    {
        _cwd_input_buff.resize(4096, 0);
        _file_name_buff.resize(4096, 0);

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

    void file_browser_widget::set_cwd(const std::filesystem::path& new_path) {
        this->_change_cwd(new_path);
    }

    void file_browser_widget::set_max_visible_items(size_t num_visible_items) {
        _max_visible_cwd_items = num_visible_items;
    }

    void file_browser_widget::register_file_filters(std::vector<std::string> glob_patterns) {
        _registered_file_filters.clear();
        for (const auto& glob_pattern : glob_patterns) {
            _registered_file_filters.emplace_back(glob_pattern);
        }
        if (_registered_file_filters.empty()) { // no filter: show all files
            _active_file_filter_idx = 0;
            this->_change_cwd(_cwd_path);
            return;
        }
        this->set_active_file_filter(0);
    }

    void file_browser_widget::set_active_file_filter(size_t index) {
        if (index >= _registered_file_filters.size()) {
            throw std::invalid_argument{ "invalid filter index" };
        }
        _active_file_filter_idx = index;
        if (!this->_test_file_filter(_sel_path)) {
            _sel_path.clear();
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(_hl_path, ec) && !this->_test_file_filter(_hl_path)) {
            _hl_path.clear(); // a highlighted folder is not subject to the file filter
        }
        this->_change_cwd(_cwd_path); // invalidate cwd items
    }

    void file_browser_widget::clear_selected_path() {
        _sel_path.clear();
        _hl_path.clear();
        _typed_name.reset();
    }

    bool file_browser_widget::show()
    {
        struct ImGuiScopedID final {
            ImGuiScopedID(const std::string& str_id) { ImGui::PushID(str_id.c_str()); }
            ~ImGuiScopedID() { ImGui::PopID(); }
        } id_scope{ _widget_id };

        bool file_was_selected_this_frame{ false };

        this->_show_cwd_bar();

        if (!_last_error.empty()) {
            ImGui::TextColored(kErrorTextColor, "Error: %s", _last_error.c_str());
        }

        const bool select_on_single_click = utility::has_flag(_flags, file_browser_flags::select_on_single_click);
        // Double clicks are reported in both modes; in single-click mode the second click of a double click is
        // ignored, since it would land on whatever item the first click brought under the cursor.
        const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns;

        std::optional<int32_t> sel_item_idx;
        bool sel_item_confirmed{ false };
        std::optional<int32_t> context_item_idx;
        // Rows keep the list box height (text + ItemSpacing.y) instead of the table's CellPadding.
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style.CellPadding.x, style.ItemSpacing.y * 0.5f));

        // The theme's outer border and header are near black; derive them from the inner column separator.
        const ImVec4 separator_color = ImGui::GetStyleColorVec4(ImGuiCol_TableBorderLight);
        const ImVec4 header_color = scale_alpha(separator_color, kHeaderAlphaScale);
        const ImVec4 highlight_color = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered); // the row hover color
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, separator_color);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, header_color);
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, scale_alpha(highlight_color, kStripeAlphaScale));

        // Header row + visible items, or the remaining height minus the bottom rows (at least 3 items)
        const bool show_open_button = !select_on_single_click;
        const float bottom_rows_height = ImGui::GetFrameHeightWithSpacing() * (show_open_button ? 2.0f : 1.0f);
        const float row_height = ImGui::GetTextLineHeightWithSpacing();
        const float table_height = _max_visible_cwd_items > 0
            ? row_height * static_cast<float>(_max_visible_cwd_items + 1)
            : std::max(row_height * 4.0f, ImGui::GetContentRegionAvail().y - bottom_rows_height);
        constexpr ImGuiTableFlags kTableFlags = 
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_NoSavedSettings; // NoSavedSettings: auto IDs are not stable across runs, and the table has no user settings to keep.
        if (ImGui::BeginTable("##cwd-items", 3, kTableFlags, ImVec2(-FLT_MIN, table_height)))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // keep the header visible
            if (_cwd_items_scroll_reset) {
                ImGui::SetScrollY(0.0f);
                _cwd_items_scroll_reset = false;
            }
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0000/00/00 00:00:00").x);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("9999.99 GB").x);

            // Headers are not interactive; keep them from turning dark on hover.
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, header_color);
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableNextColumn();
            ImGui::TableHeader("Name");
            ImGui::TableNextColumn();
            ImGui::TableHeader("Modified");
            ImGui::TableNextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Size").x);
            ImGui::TableHeader("Size");
            ImGui::PopStyleColor(2);

            // Icons differ in width; names start at the same x past the widest one.
            const float icon_slot_width = std::max({ ImGui::CalcTextSize(kIconFolder).x, ImGui::CalcTextSize(kIconFile).x,
                ImGui::CalcTextSize(kIconParentFolder).x }) + style.ItemInnerSpacing.x;

            // Selectables draw no background: a highlighted or hovered row sets its RowBg0 instead, which replaces
            // the stripe rather than blending over it (see ImGuiTableBgTarget_), so it looks the same on every row.
            ImGui::PushStyleColor(ImGuiCol_Header, kTransparentColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kTransparentColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, kTransparentColor);
            const ImU32 highlight_row_color = ImGui::GetColorU32(highlight_color);

            // Only the visible rows are submitted; the clipper also keeps keyboard navigation working across them.
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(_cwd_items.size()));
            while (clipper.Step()) {
                for (int32_t i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const file_item_t& item = _cwd_items[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    const bool is_highlighted = item.full_path == _hl_path;

                    // File names are drawn as plain text, not as the label: "##" / "###" in a name would hide text or merge IDs.
                    ImGui::PushID(i);
                    const float name_pos_x = ImGui::GetCursorPosX();
                    const bool clicked = ImGui::Selectable("##item", is_highlighted, selectable_flags);
                    if (is_highlighted || ImGui::IsItemHovered()) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, highlight_row_color);
                    }
                    const bool double_clicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    // Enter on the focused item acts like a double click.
                    const bool enter_pressed = ImGui::IsItemFocused()
                        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
                    if (enter_pressed || (clicked && !(select_on_single_click && double_clicked))) {
                        sel_item_idx = i;
                        sel_item_confirmed = select_on_single_click || enter_pressed || double_clicked;
                    }
                    if (!item.is_parent_link && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        context_item_idx = i;
                    }
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(name_pos_x);
                    const bool tint_icon = item.is_directory && !item.is_parent_link;
                    if (tint_icon) {
                        ImGui::PushStyleColor(ImGuiCol_Text, kFolderIconColor);
                    }
                    ImGui::TextUnformatted(item.is_parent_link ? kIconParentFolder : item.is_directory ? kIconFolder : kIconFile);
                    if (tint_icon) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(name_pos_x + icon_slot_width);
                    ImGui::TextUnformatted(item.display_name.c_str());
                    ImGui::PopID();

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.modified_text.c_str());

                    ImGui::TableNextColumn();
                    if (!item.size_text.empty()) {
                        // right-aligned
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(item.size_text.c_str()).x);
                        ImGui::TextUnformatted(item.size_text.c_str());
                    }

                }
            }
            ImGui::PopStyleColor(3);

            ImGui::EndTable();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        // The context menu is submitted outside the clipped rows so that scrolling cannot drop it.
        if (context_item_idx) {
            const auto& context_item = _cwd_items[*context_item_idx];
            this->_set_highlight(context_item);
            _context_item_path = context_item.full_path;
            ImGui::OpenPopup("##item-context");
        }
        if (ImGui::BeginPopup("##item-context")) {
            if (ImGui::MenuItem("Copy Path")) {
                ImGui::SetClipboardText(path_to_utf8(_context_item_path).c_str());
            }
            ImGui::EndPopup();
        }

        if (sel_item_idx.value_or(_cwd_items.size()) < _cwd_items.size())
        {
            const auto& sel_item = _cwd_items[sel_item_idx.value()];
            if (sel_item_confirmed) {
                file_was_selected_this_frame = this->_open_item(sel_item);
            } else if (sel_item.full_path == _hl_path) {
                _hl_path.clear(); // clicking the highlighted item again clears the highlight
            } else {
                this->_set_highlight(sel_item);
            }
        }

        // Bottom rows: "File Name: [name] Filter: [filter]" / "[Open]"
        const char* const filter_label = "Filter:";
        const char* const filter_preview = _registered_file_filters.empty()
            ? nullptr
            : _registered_file_filters[_active_file_filter_idx].get_glob_pattern().c_str();
        const float filter_width = filter_preview
            ? ImGui::CalcTextSize(filter_label).x + ImGui::CalcTextSize(filter_preview).x
                + style.FramePadding.x * 2.0f + ImGui::GetFrameHeight() /* arrow */ + style.ItemSpacing.x * 2.0f
            : 0.0f;

        auto find_highlighted_item = [this]() {
            return std::find_if(_cwd_items.begin(), _cwd_items.end(),
                [this](const file_item_t& item) { return item.full_path == _hl_path; });
        };

        auto highlighted_file_name = [&]() {
            const auto hl_item_it = find_highlighted_item();
            return (hl_item_it != _cwd_items.end() && !hl_item_it->is_directory) ? path_to_utf8(_hl_path.filename()) : std::string{};
        };

        // The input shows the typed name, or else the highlighted file in this folder; it is derived every frame
        // (except while being edited), so it cannot go stale when the folder or the filter changes.
        if (!_file_name_active) {
            const std::string shown_name = _typed_name ? *_typed_name : highlighted_file_name();
            std::snprintf(_file_name_buff.data(), _file_name_buff.size(), "%s", shown_name.c_str());
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("File Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(ImGui::GetFontSize() * 5.0f, ImGui::GetContentRegionAvail().x - filter_width));
        const bool name_entered = ImGui::InputText("##file-name",
            _file_name_buff.data(),
            _file_name_buff.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);
        _file_name_active = ImGui::IsItemActive();
        if (ImGui::IsItemEdited()) {
            // Text the input would show anyway (e.g. restored by Esc) is not a typed name.
            std::string text{ _file_name_buff.data() };
            _typed_name = (text.empty() || text == highlighted_file_name()) ? std::nullopt : std::optional<std::string>{ std::move(text) };
        }

        if (filter_preview) {
            ImGui::SameLine();
            ImGui::TextUnformatted(filter_label);
            ImGui::SameLine();
            if (ImGui::BeginCombo("##file-filter-dropdown",
                _registered_file_filters[_active_file_filter_idx].get_glob_pattern().c_str(),
                ImGuiComboFlags_WidthFitPreview))
            {
                for (size_t n = 0; n < _registered_file_filters.size(); ++n)
                {
                    const bool is_selected = n == _active_file_filter_idx;
                    ImGui::PushID(static_cast<int>(n)); // the same pattern may be registered twice
                    if (ImGui::Selectable(_registered_file_filters[n].get_glob_pattern().c_str(), is_selected)) {
                        this->set_active_file_filter(n);
                    }
                    ImGui::PopID();

                    // Focus on the default selected item when the dropdown opens.
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        // Found again: the filter combo above may have rebuilt the items.
        const auto hl_item_it = find_highlighted_item();
        const bool hl_item_visible = hl_item_it != _cwd_items.end();

        bool open_requested = name_entered;
        if (show_open_button) {
            // A typed name, or else the highlighted item (same as double-clicking it)
            ImGui::BeginDisabled(!_typed_name && !hl_item_visible);
            open_requested |= ImGui::Button("Open");
            ImGui::EndDisabled();
        }

        if (open_requested) {
            if (_typed_name) {
                file_was_selected_this_frame = this->_open_typed_path();
            } else if (hl_item_visible) {
                file_was_selected_this_frame = this->_open_item(*hl_item_it);
            }
        }

        return file_was_selected_this_frame;
    }

    void file_browser_widget::_show_cwd_bar()
    {
        if (_cwd_editing) {
            if (_cwd_edit_focus) {
                ImGui::SetKeyboardFocusHere();
                _cwd_edit_focus = false;
            }
            // SetKeyboardFocusHere() activates the input as keyboard navigation, which turns the nav cursor on.
            ImGui::PushStyleColor(ImGuiCol_NavCursor, kTransparentColor);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool entered = ImGui::InputText("##cwd-input",
                _cwd_input_buff.data(),
                _cwd_input_buff.size(),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopStyleColor();

            if (ImGui::IsItemActivated()) {
                ImGui::SetNavCursorVisible(false);
            }
            if (entered) {
                this->_change_cwd(this->_resolve_user_path(_cwd_input_buff.data()));
            }
            if (ImGui::IsItemDeactivated()) {
                _cwd_editing = false;
            }
            return;
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const float separator_width = ImGui::CalcTextSize(">").x + style.ItemSpacing.x * 2.0f;
        const float places_button_width = calc_button_width(kIconPlaces);
        const float edit_button_width = calc_button_width(kIconEditPath);
        const float avail_width = ImGui::GetContentRegionAvail().x
            - places_button_width - edit_button_width - style.ItemSpacing.x * 2.0f;
        const size_t num_segments = _cwd_segments.size();

        // Width of segments [first, num_segments), plus the "..." button when leading segments are hidden
        auto calc_segments_width = [&](const size_t first) {
            float width = first > 0 ? calc_button_width("...") + separator_width : 0.0f;
            for (size_t i = first; i < num_segments; ++i) {
                width += ImGui::CalcTextSize(_cwd_segments[i].label.c_str(), nullptr, true).x + (i > first ? separator_width : 0.0f);
            }
            return width;
        };

        // Hide leading segments until the rest fits; the last segment is always shown.
        size_t first_segment = num_segments > 0 ? num_segments - 1 : 0;
        if (calc_segments_width(0) <= avail_width) {
            first_segment = 0;
        } else {
            while (first_segment > 1 && calc_segments_width(first_segment - 1) <= avail_width) {
                --first_segment;
            }
        }

        std::optional<std::filesystem::path> nav_path;

        // Places: the home folder and the file system roots
        if (ImGui::Button(kIconPlaces)) {
            _home_dir = find_home_dir();
            _root_dirs = list_root_dirs();
            ImGui::OpenPopup("##places");
        }
        ImGui::SetItemTooltip("Places");
        if (ImGui::BeginPopup("##places")) {
            if (!_home_dir.empty()) {
                if (ImGui::Selectable((std::string{ kIconHome } + "  Home").c_str())) {
                    nav_path = _home_dir;
                }
                if (!_root_dirs.empty()) {
                    ImGui::Separator();
                }
            }
            for (size_t i = 0; i < _root_dirs.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable((std::string{ kIconPlaces } + "  " + path_to_utf8(_root_dirs[i])).c_str())) {
                    nav_path = _root_dirs[i];
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        if (first_segment > 0) {
            if (ImGui::Button("...")) {
                ImGui::OpenPopup("##hidden-segments");
            }
            if (ImGui::BeginPopup("##hidden-segments")) {
                for (size_t i = 0; i < first_segment; ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Selectable(_cwd_segments[i].label.c_str())) {
                        nav_path = _cwd_segments[i].path;
                    }
                    ImGui::PopID();
                }
                ImGui::EndPopup();
            }
        }

        for (size_t i = first_segment; i < num_segments; ++i) {
            if (i > 0) {
                ImGui::SameLine();
                ImGui::TextUnformatted(">");
                ImGui::SameLine();
            }
            if (i + 1 == num_segments) {
                ImGui::TextUnformatted(_cwd_segments[i].label.c_str()); // current folder
            } else {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TextLink(_cwd_segments[i].label.c_str())) {
                    nav_path = _cwd_segments[i].path;
                }
                ImGui::PopID();
            }
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - edit_button_width));
        if (ImGui::Button(kIconEditPath)) {
            std::snprintf(_cwd_input_buff.data(), _cwd_input_buff.size(), "%s", path_to_utf8(_cwd_path).c_str());
            _cwd_editing = true;
            _cwd_edit_focus = true;
        }
        ImGui::SetItemTooltip("Edit path");

        if (nav_path) {
            this->_change_cwd(*nav_path);
        }
    }

    bool file_browser_widget::_open_item(const file_item_t& item)
    {
        if (item.is_directory) {
            this->_change_cwd(item.full_path);
            return false;
        }
        this->_set_highlight(item);
        _sel_path = item.full_path;
        return true;
    }

    bool file_browser_widget::_open_typed_path()
    {
        const std::filesystem::path target_path = this->_resolve_user_path(_typed_name.value_or(std::string{}));

        std::error_code ec;
        if (std::filesystem::is_directory(target_path, ec)) {
            _typed_name.reset();
            this->_change_cwd(target_path);
            return false;
        }
        if (!std::filesystem::is_regular_file(target_path, ec)) {
            _last_error = "File not found: " + path_to_utf8(target_path);
            return false;
        }
        _last_error.clear();
        _typed_name.reset();
        _hl_path = target_path;
        _sel_path = target_path;
        return true;
    }

    // Picking an item in the list replaces whatever name was typed.
    void file_browser_widget::_set_highlight(const file_item_t& item)
    {
        _hl_path = item.full_path;
        _typed_name.reset();
    }

    // Both inputs (path bar, file name) resolve relative paths against the browsing directory.
    std::filesystem::path file_browser_widget::_resolve_user_path(std::string_view text) const
    {
        const std::filesystem::path path = utf8_to_path(trim_user_path(text));
        return (path.is_absolute() ? path : _cwd_path / path).lexically_normal();
    }

    void file_browser_widget::_change_cwd(std::filesystem::path new_dir_path)
    {
        if (new_dir_path.empty()) {
            _last_error = "Path cannot be empty.";
            return;
        }

        // Resolve symlinks and "." / ".." where possible; otherwise the path is used as given.
        std::error_code ec;
        if (auto canonical_path = std::filesystem::weakly_canonical(new_dir_path, ec); !ec) {
            new_dir_path = std::move(canonical_path);
        }

        // An invalid path keeps the current directory and its items.
        if (!std::filesystem::is_directory(new_dir_path, ec)) {
            _last_error = "Invalid directory path: " + path_to_utf8(new_dir_path);
            return;
        }
        _last_error.clear();

        // Clear the current working directory items
        _cwd_items.clear();
        _cwd_items_scroll_reset = true;

        // Update the current working directory path
        _cwd_path = new_dir_path;

        // Rebuild the path segments, e.g. "C:\a\b" -> "C:", "a", "b" / "/a/b" -> "/", "a", "b"
        _cwd_segments.clear();
        std::filesystem::path segment_path = _cwd_path.root_path();
        if (!segment_path.empty()) {
            const std::filesystem::path root_label = _cwd_path.has_root_name() ? _cwd_path.root_name() : _cwd_path.root_directory();
            _cwd_segments.push_back({ path_to_utf8(root_label), segment_path });
        }
        for (const auto& part : _cwd_path.relative_path()) {
            if (part.empty()) { continue; } // trailing separator
            segment_path /= part;
            _cwd_segments.push_back({ path_to_utf8(part), segment_path });
        }

        // Update the current working directory items ...
        // add ".." item to cwd items (if the current path has a parent)
        if (_cwd_path.has_parent_path()) {
            std::filesystem::path parent_p = _cwd_path.parent_path();
            // Prevent moving to parent of root directory (e.g., C:\ to C:\)
            if (parent_p != _cwd_path) {
                // Add ".." only if the parent path is different from the current path
                // Check actual path using canonical (considering symlinks, etc.)
                const std::filesystem::path canonical_parent = std::filesystem::canonical(parent_p, ec);
                if (ec) {
                    _last_error = "Could not add '..' entry: " + ec.message();
                } else if (canonical_parent != std::filesystem::canonical(_cwd_path, ec)) { // a failure yields an empty path
                    _cwd_items.emplace_back("..", canonical_parent, true, true);
                }
            }
        }

        std::vector<file_item_t> temp_dir_items, temp_file_items;

        // Errors are handled per entry: one unreadable entry is skipped instead of dropping the whole listing.
        std::filesystem::directory_iterator dir_it{ _cwd_path, std::filesystem::directory_options::skip_permission_denied, ec };
        if (ec) {
            _last_error = "Error accessing directory contents: " + ec.message();
            return; // only ".." is listed
        }
        for (const std::filesystem::directory_iterator dir_end; !ec && dir_it != dir_end; dir_it.increment(ec))
        {
            const std::filesystem::directory_entry& dir_entry = *dir_it;
            std::string entry_filename_str = path_to_utf8(dir_entry.path().filename());
            if (entry_filename_str.empty() || entry_filename_str == "." || entry_filename_str == "..")
            {
                continue;
            }

            std::error_code entry_ec;
            file_item_t* new_item = nullptr;
            if (dir_entry.is_directory(entry_ec))
            {
                new_item = &temp_dir_items.emplace_back(entry_filename_str, dir_entry.path(), true);
            }
            else if (dir_entry.is_regular_file(entry_ec))
            {
                if (this->_test_file_filter(dir_entry.path())) {
                    new_item = &temp_file_items.emplace_back(entry_filename_str, dir_entry.path(), false);
                    std::error_code size_ec;
                    const uintmax_t file_size = dir_entry.file_size(size_ec);
                    if (!size_ec) {
                        new_item->size_text = format_file_size(file_size);
                    }
                }
            }

            if (new_item) {
                std::error_code time_ec;
                const auto file_time = dir_entry.last_write_time(time_ec);
                if (!time_ec) {
                    new_item->modified_text = format_file_time(file_time);
                }
            }
        }
        if (ec) {
            _last_error = "Directory listing stopped early: " + ec.message(); // keep what was read so far
        }

        // sort directories and files separately, by actual name
        auto sort_rule = [](const file_item_t& a, const file_item_t& b) {
            return natural_less(a.display_name, b.display_name);
        };
        std::sort(temp_dir_items.begin(), temp_dir_items.end(), sort_rule);
        std::sort(temp_file_items.begin(), temp_file_items.end(), sort_rule);

        _cwd_items.insert(_cwd_items.end(), temp_dir_items.begin(), temp_dir_items.end());
        _cwd_items.insert(_cwd_items.end(), temp_file_items.begin(), temp_file_items.end());
    }

    bool file_browser_widget::_test_file_filter(const std::filesystem::path& file_path) const
    {
        if (_registered_file_filters.empty()) {
            return true; // No filters, show all files
        }

        const auto& active_filter = _registered_file_filters.at(_active_file_filter_idx);
        return active_filter.matches(file_path);
    }

} // namespace
