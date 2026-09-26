#include "log_window.hh"

#include <triengine/common.h>
#include <triengine/global_options.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <deque>
#include <mutex>
#include <vector>

namespace triengine::gui
{
    namespace
    {
        ImVec4 level_color(const utility::log_level lv)
        {
            switch (lv) {
            case utility::log_level::trace:    return ImVec4{ 0.60f, 0.60f, 0.60f, 1.0f }; // gray
            case utility::log_level::debug:    return ImVec4{ 0.40f, 0.80f, 1.00f, 1.0f }; // cyan
            case utility::log_level::info:     return ImVec4{ 0.55f, 0.85f, 0.45f, 1.0f }; // green
            case utility::log_level::warn:     return ImVec4{ 1.00f, 0.80f, 0.25f, 1.0f }; // yellow
            case utility::log_level::error:    return ImVec4{ 1.00f, 0.40f, 0.40f, 1.0f }; // red
            case utility::log_level::critical: return ImVec4{ 1.00f, 0.30f, 0.85f, 1.0f }; // magenta
            default:                           return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            }
        }

        const char* level_tag(const utility::log_level lv)
        {
            switch (lv) {
            case utility::log_level::trace:    return "TRC";
            case utility::log_level::debug:    return "DBG";
            case utility::log_level::info:     return "INF";
            case utility::log_level::warn:     return "WRN";
            case utility::log_level::error:    return "ERR";
            case utility::log_level::critical: return "CRT";
            default:                           return "???";
            }
        }

        constexpr uint32_t make_level_mask(const utility::log_level min_lv)
        {
            uint32_t mask{ 0 };
            for (size_t i = static_cast<size_t>(min_lv); i < utility::kNumLogLevels; ++i) {
                mask |= (1u << i);
            }
            return mask;
        }

        // Bounds for the record capacity
        constexpr size_t kMinLogCapacity{ 100 };
        constexpr size_t kMaxLogCapacity{ 1000000 };

        // Levels shown before the user touches the toggles.
        constexpr utility::log_level kDefaultMinLevel{
#if defined(TRIENGINE_DEBUG_MODE)
            utility::log_level::debug
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
            utility::log_level::info
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^
        };

        std::string make_timestamp()
        {
            using clock_type = std::chrono::system_clock;

            const clock_type::time_point now = clock_type::now();
            const std::time_t now_time = clock_type::to_time_t(now);
            const auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::tm local_time{};
            ::localtime_s(&local_time, &now_time);

            return utility::string::c_format("%02d:%02d:%02d.%03d"
                , local_time.tm_hour
                , local_time.tm_min
                , local_time.tm_sec
                , static_cast<int>(msec.count())
            );
        }

        // Turns one message into the lines the console shows. `ImGuiListClipper` assumes every row has the same height, 
        // so a message with embedded newlines becomes several lines here instead of one row several lines tall.
        std::vector<std::string> format_log_lines(const utility::log_level lv, const std::string_view msg_sv)
        {
            const std::string timestamp = make_timestamp(); // one reading for the whole message

            const auto format_first_line = [&timestamp, lv](const std::string_view line_sv) {
                return utility::string::c_format("[%s] [%s] %.*s"
                    , timestamp.c_str()
                    , level_tag(lv)
                    , static_cast<int>(line_sv.size())
                    , line_sv.data()
                );
            };

            // Lines after the first carry no timestamp or tag; they are indented under the first one.
            const auto format_continuation_line = [](const std::string_view line_sv) {
                return utility::string::c_format("    %.*s"
                    , static_cast<int>(line_sv.size())
                    , line_sv.data()
                );
            };

            std::vector<std::string> lines;
            size_t line_begin{ 0 };

            while (true)
            {
                const size_t newline_pos = msg_sv.find('\n', line_begin);
                const bool is_last_line = (newline_pos == std::string_view::npos);
                const size_t line_end = is_last_line ? msg_sv.size() : newline_pos;

                std::string_view line_sv = msg_sv.substr(line_begin, line_end - line_begin);
                if (!line_sv.empty() && line_sv.back() == '\r') { line_sv.remove_suffix(1); }

                // a trailing newline must not add an empty line
                if (lines.empty() || !(is_last_line && line_sv.empty()))
                {
                    lines.push_back(lines.empty()
                        ? format_first_line(line_sv)
                        : format_continuation_line(line_sv)
                    );
                }

                if (is_last_line) { break; }
                line_begin = line_end + 1;
            }

            return lines;
        }

        struct log_record_t
        {
            utility::log_level level{ utility::log_level::info };
            std::string text; // formatted line: timestamp, level tag, message
        };

        // Records handed over by the logging threads, drained by the render thread.
        class pending_record_buffer {
        public:
            // Appends the lines of one message at once, so they cannot interleave with another
            // thread's, then drops the oldest records past the capacity.
            void push_records(std::vector<log_record_t>&& records) {
                std::scoped_lock lk{ _lock };
                for (log_record_t& record : records) {
                    _records_q.push_back(std::move(record));
                }
                this->_trim();
            }

            // Hands over everything pending so far, leaving the buffer empty.
            std::deque<log_record_t> take_records() {
                std::deque<log_record_t> taken; {
                    std::scoped_lock lk{ _lock };
                    taken.swap(_records_q);
                }
                return taken;
            }

            void clear() {
                std::scoped_lock lk{ _lock };
                _records_q.clear();
            }

            void set_capacity(const size_t capacity) {
                std::scoped_lock lk{ _lock };
                _capacity = (capacity > 0) ? capacity : 1;
                this->_trim();
            }

        private:
            // call with `_lock` held
            void _trim() {
                while (_records_q.size() > _capacity) {
                    _records_q.pop_front();
                }
            }

            std::mutex _lock;
            std::deque<log_record_t> _records_q;
            size_t _capacity{ log_window::kDefaultLogCapacity };
        }; // class pending_record_buffer

        // Splits `msg_sv` into one record per line and hands them to the pending buffer. (thread-safe)
        void push_log_lines(
            pending_record_buffer& pending_record_buff,
            const utility::log_level lv,
            const std::string_view msg_sv)
        {
            // formatted before taking the lock; the buffer only splices the finished lines in
            std::vector<std::string> lines = format_log_lines(lv, msg_sv);

            std::vector<log_record_t> records;
            records.reserve(lines.size());
            for (std::string& line : lines) {
                records.push_back(log_record_t{ lv, std::move(line) });
            }

            pending_record_buff.push_records(std::move(records));
        }

    } // namespace

    struct log_window::impl_t
    {
        // --- cross-thread handoff ---
        // NOTE: Held through a `shared_ptr` so that a record in flight stays safe while the window dies.
        std::shared_ptr<pending_record_buffer> pending_record_buff{ std::make_shared<pending_record_buffer>() };

        // NOTE: drops the engine subscription on destruction
        utility::logger::subscription engine_log_subscription;

        // --- record storage ---
        std::deque<log_record_t> record_buff; // everything the console keeps, oldest first
        size_t record_buff_capacity{ log_window::kDefaultLogCapacity };
        uint64_t record_buff_version{ 0 }; // bumps whenever `record_buff` changes
        std::array<size_t, utility::kNumLogLevels> per_level_record_counts{}; // derived from `record_buff`

        // --- view state ---
        ImGuiTextFilter text_filter;
        uint32_t shown_level_mask{ make_level_mask(kDefaultMinLevel) }; // bit k = level k is shown
        bool flag_autoscroll{ true }; // keep scrolling if already at the bottom

        // Indices into `record_buff` that pass the filters.
        // Rebuilt only when the buffer or a filter changed, so the list clipper still drives the filtered view.
        std::vector<size_t> visible_record_indices;
        uint64_t visible_built_version{ 0 }; // buffer version `visible_record_indices` was built from
        bool flag_visible_dirty{ true };

    public:
        impl_t() = default;

        bool is_level_shown(const utility::log_level lv) const {
            return (shown_level_mask & (1u << static_cast<uint32_t>(lv))) != 0;
        }

        void clear_records()
        {
            pending_record_buff->clear();

            record_buff.clear();
            per_level_record_counts.fill(0);
            ++record_buff_version;
        }

        // Keeps the pending buffer on the same capacity, so neither side outgrows the other.
        void set_record_capacity(const size_t capacity)
        {
            const size_t effective_capacity = std::clamp(capacity, kMinLogCapacity, kMaxLogCapacity);
            pending_record_buff->set_capacity(effective_capacity);
            record_buff_capacity = effective_capacity;
            this->trim_record_buff_to_capacity();
        }

        void drain_pending_records()
        {
            std::deque<log_record_t> pending_records_q = pending_record_buff->take_records();
            if (pending_records_q.empty()) { return; }

            for (log_record_t& record : pending_records_q) {
                per_level_record_counts[static_cast<size_t>(record.level)]++;
                record_buff.push_back(std::move(record));
            }

            this->trim_record_buff_to_capacity();
            ++record_buff_version;
        }

        void trim_record_buff_to_capacity()
        {
            if (record_buff.size() <= record_buff_capacity) { return; }

            while (record_buff.size() > record_buff_capacity) {
                per_level_record_counts[static_cast<size_t>(record_buff.front().level)]--;
                record_buff.pop_front();
            }

            // dropping from the front shifts every index, so the filtered view is stale now
            ++record_buff_version;
        }

        // Rebuilds the filtered view when the record buffer or a filter moved on, 
        // and remembers which buffer version it was built from so the next frame can tell.
        void ensure_visible_record_indices()
        {
            const bool needs_rebuild = flag_visible_dirty || (visible_built_version != record_buff_version);
            if (!needs_rebuild) { return; }

            visible_record_indices.clear();
            visible_record_indices.reserve(record_buff.size());

            for (size_t i = 0; i < record_buff.size(); ++i)
            {
                const log_record_t& record = record_buff[i];
                if (!this->is_level_shown(record.level)) { continue; }
                if (!text_filter.PassFilter(record.text.c_str())) { continue; }
                visible_record_indices.push_back(i);
            }

            visible_built_version = record_buff_version;
            flag_visible_dirty = false;
        }

        // `do_copy` is set when clipboard copy button was pressed.
        void draw_toolbar(bool& do_copy)
        {
            if (ImGui::BeginPopup("Options")) {
                ImGui::Checkbox("Auto-scroll", &flag_autoscroll);

                int capacity = static_cast<int>(record_buff_capacity);
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::DragInt("Capacity", &capacity, 10.0f
                    , static_cast<int>(kMinLogCapacity), static_cast<int>(kMaxLogCapacity)
                    , "%d records"
                    , ImGuiSliderFlags_AlwaysClamp))
                {
                    this->set_record_capacity(static_cast<size_t>(capacity));
                }
                ImGui::EndPopup();
            }

            if (ImGui::Button("Options")) {
                ImGui::OpenPopup("Options");
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                this->clear_records();
            }

            ImGui::SameLine();
            do_copy = ImGui::Button("Copy");

            for (size_t i = 0; i < utility::kNumLogLevels; ++i)
            {
                const auto lv = static_cast<utility::log_level>(i);
                bool shown = this->is_level_shown(lv);

                ImGui::SameLine();
                const std::string label = utility::string::c_format("%s %zu###lv%zu", level_tag(lv), per_level_record_counts[i], i);
                if (ImGui::Checkbox(label.c_str(), &shown)) {
                    shown_level_mask ^= (1u << static_cast<uint32_t>(lv));
                    flag_visible_dirty = true;
                }
                ImGui::SetItemTooltip("%s: %zu record(s) buffered", level_tag(lv), per_level_record_counts[i]);
            }

            ImGui::SameLine();
            if (text_filter.Draw("Filter", -100.0f)) {
                flag_visible_dirty = true;
            }
        }
    };

    log_window::log_window(
        const bool capture_engine_logs,
        const size_t capacity)
        : _impl{ std::make_unique<impl_t>() }
    {
        _impl->set_record_capacity(capacity);

        if (capture_engine_logs)
        {
            _impl->engine_log_subscription = global_options::instance()->get_logger().subscribe(
                [pending_record_buff = _impl->pending_record_buff](const utility::log_level lv, const std::string& msg)
            {
                push_log_lines(*pending_record_buff, lv, msg);
            });
        }
    }

    log_window::~log_window() = default;

    void log_window::add_log(
        const utility::log_level lv,
        const std::string_view sv)
    {
        push_log_lines(*_impl->pending_record_buff, lv, sv);
    }

    void log_window::render(
        [[maybe_unused]] const window_render_context& render_ctx)
    {
        _impl->drain_pending_records();

        bool do_copy{ false };
        _impl->draw_toolbar(do_copy);

        ImGui::Separator();

        if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            _impl->ensure_visible_record_indices();

            // Copy takes the whole filtered view, not just the rows the clipper drew.
            if (do_copy)
            {
                std::string all_text;
                for (const size_t record_idx : _impl->visible_record_indices) {
                    all_text += _impl->record_buff[record_idx].text;
                    all_text += '\n';
                }
                ImGui::SetClipboardText(all_text.c_str());
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(_impl->visible_record_indices.size()));
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const log_record_t& record = _impl->record_buff[_impl->visible_record_indices[static_cast<size_t>(i)]];
                    ImGui::PushStyleColor(ImGuiCol_Text, level_color(record.level));
                    ImGui::TextUnformatted(record.text.c_str());
                    ImGui::PopStyleColor();
                }
            }
            clipper.End();

            ImGui::PopStyleVar();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (_impl->flag_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }

        ImGui::EndChild();
    }

} // namespace
