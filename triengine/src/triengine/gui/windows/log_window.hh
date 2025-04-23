#pragma once
#include <triengine/gui/iwindow.hh>

#include <string>
#include <string_view>
#include <type_traits>

namespace triengine::gui
{
    // Refs:
    // https://github.com/ocornut/imgui/issues/300
    // https://github.com/ocornut/imgui/blob/97a1111b94c7bb989e393c9a11b647c7585ac72e/imgui_demo.cpp#L7491
    // https://github.com/ocornut/imgui/blob/97a1111b94c7bb989e393c9a11b647c7585ac72e/imgui_demo.cpp#L7126

    class log_window
        : public iwindow
    {
    public:
        //enum class LogLevel : char {
        //    Trace = 'T',
        //    Debug = 'D',
        //    Info = 'I',
        //    Warn = 'W',
        //    Err = 'E',
        //    Critical = 'C',
        //};

    private:
        // See: https://gist.github.com/ConnerWill/d4b6c776b509add763e17f9f113fd25b
        static constexpr char kAnsiEscapeCode = 0x1B;

    private:
        ImGuiTextBuffer _text_buff;
        ImGuiTextFilter _text_filter;
        ImVector<int> _line_offsets; // Index to lines offset. We maintain this with AddLog() calls.
        bool _flag_autoscroll = true;  // Keep scrolling if already at the bottom.

    public:
        log_window() { this->clear_logs(); }
        virtual ~log_window() = default;

        const char* get_window_name() const override {
            return "Logs";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 450.0f, 300.0f };
        }

        void clear_logs()
        {
            _text_buff.clear();
            _line_offsets.clear();
            _line_offsets.push_back(0);
        }

        void add_log(
            //LogLevel lv, 
            std::string_view sv)
        {
            int old_size = _text_buff.size();
            //_text_buff.appendf("\x1B<%c>", static_cast<std::underlying_type_t<LogLevel>>(lv)); // custom ANSI escape sequence
            _text_buff.append(sv.data(), sv.data() + sv.size());
            _text_buff.append("\n");

            for (int new_size = _text_buff.size(); old_size < new_size; ++old_size) {
                if (_text_buff[old_size] == '\n') {
                    _line_offsets.push_back(old_size + 1);
                }
            }
        }

        template <typename... _Args>
        void add_log(
            //LogLevel lv, 
            const std::string& c_fmt, 
            _Args&&... args)
        {
            int old_size = _text_buff.size();
            //_text_buff.appendf("\x1B<%c>", static_cast<std::underlying_type_t<LogLevel>>(lv)); // custom ANSI escape sequence
            _text_buff.appendf(c_fmt.c_str(), std::forward<_Args>(args)...);
            _text_buff.append("\n");

            for (int new_size = _text_buff.size(); old_size < new_size; ++old_size) {
                if (_text_buff[old_size] == '\n') {
                    _line_offsets.push_back(old_size + 1);
                }
            }
        }

        void render(
            [[maybe_unused]] const window_render_context& render_ctx) override
        {
            // Options menu
            if (ImGui::BeginPopup("Options")) {
                ImGui::Checkbox("Auto-scroll", &_flag_autoscroll);
                ImGui::EndPopup();
            }

            // Main window
            if (ImGui::Button("Options")) {
                ImGui::OpenPopup("Options");
            }

            ImGui::SameLine();
            const bool clear = ImGui::Button("Clear");

            ImGui::SameLine();
            const bool copy = ImGui::Button("Copy");

            ImGui::SameLine();
            _text_filter.Draw("Filter", -100.0f);

            ImGui::Separator();

            if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
            {
                if (clear) {
                    this->clear_logs();
                }

                if (copy) {
                    ImGui::LogToClipboard();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                const char* const buff_begin = _text_buff.begin();
                const char* const buff_end = _text_buff.end();

                if (_text_filter.IsActive())
                {
                    // In this example we don't use the clipper when Filter is enabled.
                    // This is because we don't have random access to the result of our filter.
                    // A real application processing logs with ten of thousands of entries may want to store the result of
                    // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
                    for (int line_no = 0; line_no < _line_offsets.Size; ++line_no)
                    {
                        const char* const line_begin = buff_begin + _line_offsets[line_no];
                        const char* const line_end = (line_no + 1 < _line_offsets.Size) ? (buff_begin + _line_offsets[line_no + 1] - 1) : buff_end;
                        if (_text_filter.PassFilter(line_begin, line_end)) {
                            ImGui::TextUnformatted(line_begin, line_end);
                        }
                    }
                }
                else
                {
                    // The simplest and easy way to display the entire buffer:
                    //   ImGui::TextUnformatted(buff_begin, buff_end);
                    // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
                    // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
                    // within the visible area.
                    // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
                    // on your side is recommended. Using ImGuiListClipper requires
                    // - A) random access into your data
                    // - B) items all being the  same height,
                    // both of which we can handle since we have an array pointing to the beginning of each line of text.
                    // When using the filter (in the block of code above) we don't have random access into the data to display
                    // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
                    // it possible (and would be recommended if you want to search through tens of thousands of entries).

                    ImGuiListClipper clipper;
                    clipper.Begin(_line_offsets.Size);
                    while (clipper.Step()) {
                        for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; ++line_no) {
                            const char* const line_begin = buff_begin + _line_offsets[line_no];
                            const char* const line_end = (line_no + 1 < _line_offsets.Size) ? (buff_begin + _line_offsets[line_no + 1] - 1) : buff_end;

                            //ImVec4 color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                            //bool has_color = true;
                            //if (line_begin != line_end &&
                            //    *line_begin == kAnsiEscapeCode)
                            //{
                            //    has_color = true;
                            //
                            //}
                            //if (has_color) { ImGui::PushStyleColor(ImGuiCol_Text, color); }
                            ImGui::TextUnformatted(line_begin, line_end);
                            //if (has_color) { ImGui::PopStyleColor(); }

                        }
                    }
                    clipper.End();
                }

                ImGui::PopStyleVar();

                // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
                // Using a scrollbar or mouse-wheel will take away from the bottom edge.
                if (_flag_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }

            ImGui::EndChild();
        }

    private:


    }; // class

} // namespace