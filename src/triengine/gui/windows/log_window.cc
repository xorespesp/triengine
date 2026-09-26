#include "log_window.hh"

#include <triengine/common.h>
#include <triengine/global_options.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/noncopyable.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <deque>
#include <mutex>

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

        // Walks the '\n'-separated lines of `msg_sv` and calls `fn(line_sv, is_last_line)` on each.
        template <typename _Fn>
        void for_each_line(const std::string_view msg_sv, _Fn&& fn)
        {
            size_t line_begin{ 0 };

            while (true)
            {
                const size_t newline_pos = msg_sv.find('\n', line_begin);
                const bool is_last_line = (newline_pos == std::string_view::npos);
                const size_t line_end = is_last_line ? msg_sv.size() : newline_pos;

                std::string_view line_sv = msg_sv.substr(line_begin, line_end - line_begin);
                if (!line_sv.empty() && line_sv.back() == '\r') { line_sv.remove_suffix(1); }

                fn(line_sv, is_last_line);

                if (is_last_line) { break; }
                line_begin = line_end + 1;
            }
        }

        using clock_type = std::chrono::system_clock;

        struct record_t
        {
            utility::log_level level{ utility::log_level::info };
            std::string text; // the line as shown, with its head or indent

            // Starts a message: "[HH:MM:SS.mmm] [LVL] " followed by `line_sv`, stamped with `stamp`.
            static record_t make(
                const clock_type::time_point stamp,
                const utility::log_level lv,
                const std::string_view line_sv)
            {
                const std::time_t stamp_time = clock_type::to_time_t(stamp);
                const auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(stamp.time_since_epoch()) % 1000;

                std::tm local_time{};
                ::localtime_s(&local_time, &stamp_time);

                utility::string::format_string_builder<32> head;
                head.appendf("[%02d:%02d:%02d.%03d] [%s] "
                    , local_time.tm_hour
                    , local_time.tm_min
                    , local_time.tm_sec
                    , static_cast<int>(msec.count())
                    , level_tag(lv)
                );

                std::string text;
                text.reserve(head.size() + line_sv.size());
                text.append(head.view());
                text.append(line_sv);

                return record_t{ lv, std::move(text) };
            }

            // Carries on the message above it: no timestamp or tag, indented under its first line.
            static record_t make_continuation(
                const utility::log_level lv,
                const std::string_view line_sv)
            {
                constexpr std::string_view kIndent{ "    " };

                std::string text;
                text.reserve(kIndent.size() + line_sv.size());
                text.append(kIndent);
                text.append(line_sv);

                return record_t{ lv, std::move(text) };
            }
        };

        // Records handed over by the logging threads, drained by the render thread.
        class pending_record_queue {
        public:
            // Appends a message, stamped with the time and level. A multi-line message stays together
            // even when other threads log at the same time. Drops the oldest records past the capacity. (thread-safe)
            void enqueue(
                const utility::log_level lv,
                const std::string_view msg_sv)
            {
                const utility::log_level valid_lv = (static_cast<size_t>(lv) < utility::kNumLogLevels)
                    ? lv
                    : utility::log_level::critical; // fallback

                std::scoped_lock lk{ _lock };

                // One reading for the whole message. Reading the clock under the lock makes the readings
                // follow queue order across threads, so the timestamps do too unless the wall clock
                // itself is set back.
                const clock_type::time_point now = clock_type::now();

                // One record per line: `ImGuiListClipper` assumes every row has the same height,
                // so a message with embedded newlines cannot be one row several lines tall.
                bool is_first_line{ true };
                for_each_line(msg_sv, [&](const std::string_view line_sv, const bool is_last_line)
                {
                    // a trailing newline must not add an empty line
                    if (!is_first_line && is_last_line && line_sv.empty()) { return; }

                    _records_q.push_back(is_first_line
                        ? record_t::make(now, valid_lv, line_sv)
                        : record_t::make_continuation(valid_lv, line_sv));
                    is_first_line = false;
                });

                this->_trim();
            }

            // Hands over everything pending so far, leaving the queue empty.
            std::deque<record_t> dequeue_all() {
                std::deque<record_t> taken; {
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
            // Drops the oldest records until the queue fits its capacity. (call with `_lock` held)
            void _trim() {
                while (_records_q.size() > _capacity) {
                    _records_q.pop_front();
                }
            }

        private:
            std::mutex _lock;
            std::deque<record_t> _records_q;
            size_t _capacity{ log_window::kDefaultLogCapacity };
        }; // class pending_record_queue

        // Record sequence number: unique for the window's lifetime and increasing in arrival order.
        using record_seq_id_t = uint64_t;

        // Everything the console keeps, oldest first. (gui thread)
        // Records are referred to by `record_seq_id_t`; a record's position in the storage is `seq - first_seq()`.
        class record_storage {
        public:
            void append(std::deque<record_t>&& records) {
                for (record_t& record : records) {
                    _per_level_counts[static_cast<size_t>(record.level)]++;
                    _records.push_back(std::move(record));
                }
                this->_trim();
            }

            void clear() {
                // the numbering carries on past the cleared records, so nothing that is gone can be
                // mistaken for something that arrives next
                _first_seq += _records.size();
                _records.clear();
                _per_level_counts.fill(0);
            }

            void set_capacity(const size_t capacity) {
                _capacity = capacity;
                this->_trim();
            }

            size_t capacity() const {
                return _capacity;
            }

            size_t level_count(const utility::log_level lv) const {
                return _per_level_counts[static_cast<size_t>(lv)];
            }

            // The records held are numbered [`first_seq()`, `end_seq()`).
            record_seq_id_t first_seq() const { return _first_seq; }
            record_seq_id_t end_seq() const { return _first_seq + _records.size(); }

            const record_t& at_seq(const record_seq_id_t seq) const {
                TRIENGINE_ASSERT(seq >= _first_seq && seq < this->end_seq());
                return _records[static_cast<size_t>(seq - _first_seq)];
            }

        private:
            // Drops the oldest records until the storage fits its capacity.
            void _trim() {
                while (_records.size() > _capacity) {
                    // Update the per-level counts and the numbering along with every record that leaves,
                    // so that neither falls out of step with the storage. (`clear()` does the same)
                    _per_level_counts[static_cast<size_t>(_records.front().level)]--;
                    _records.pop_front();
                    ++_first_seq;
                }
            }

        private:
            std::deque<record_t> _records;
            size_t _capacity{ log_window::kDefaultLogCapacity };
            std::array<size_t, utility::kNumLogLevels> _per_level_counts{};
            // Sequence of `_records.front()`, the oldest record held, and the base the other seqs are counted from:
            //   `_records[i]` has sequence `_first_seq + i`.
            // Advances by one for every record dropped or cleared, so it also equals how many records have left the storage so far.
            record_seq_id_t _first_seq{ 0 };
        }; // class record_storage

        // The records that pass the console's filters, together with the filters themselves. (gui thread)
        class filtered_record_view : utility::noncopyable {
        public:
            explicit filtered_record_view(const record_storage& storage)
                : _storage{ storage }
            {}

            bool is_level_shown(const utility::log_level lv) const {
                return (_shown_level_mask & level_bit(lv)) != 0;
            }

            void set_level_shown(const utility::log_level lv, const bool shown) {
                const uint32_t new_mask = shown
                    ? (_shown_level_mask | level_bit(lv))
                    : (_shown_level_mask & ~level_bit(lv));
                if (new_mask == _shown_level_mask) { return; }

                _shown_level_mask = new_mask;
                _flag_dirty = true;
            }

            // Draws the input box of the text filter. Returns whether the filter changed.
            bool draw_text_filter(const char* const label, const float width) {
                if (!_text_filter.Draw(label, width)) { return false; }

                _flag_dirty = true;
                return true;
            }

            // Brings the list up to date with the storage. Call it before reading the list.
            void refresh()
            {
                // Whether a record passes is decided by its level and text alone, so it can only change
                // when a filter does. That is the one case that examines every record again.
                if (_flag_dirty) {
                    _seqs.clear();
                    _scanned_end_seq = _storage.first_seq();
                    _flag_dirty = false;
                }

                // The list keeps the seqs of the records that passed, as keys to find them by.
                // Seqs only increase, so the list stays sorted and the entries of dropped records
                // (below `first_seq()`) are always at its front.
                const record_seq_id_t first_seq = _storage.first_seq();
                while (!_seqs.empty() && _seqs.front() < first_seq) {
                    _seqs.pop_front();
                }

                // Every seq from `_scanned_end_seq` on is still unexamined: the records that arrived since
                // the last call, or every record after a filter change. Only those are examined.
                const record_seq_id_t end_seq = _storage.end_seq();
                for (record_seq_id_t seq = std::max(_scanned_end_seq, first_seq); seq < end_seq; ++seq)
                {
                    const record_t& record = _storage.at_seq(seq);
                    if (!this->is_level_shown(record.level)) { continue; }
                    if (!_text_filter.PassFilter(record.text.c_str())) { continue; }
                    _seqs.push_back(seq);
                }
                _scanned_end_seq = end_seq;
            }

            // NOTE: reflects the last `refresh()`.
            size_t size() const {
                return _seqs.size();
            }

            // NOTE: reflects the last `refresh()`.
            const record_t& at(const size_t idx) const {
                TRIENGINE_ASSERT(idx < _seqs.size());
                return _storage.at_seq(_seqs[idx]);
            }

        private:
            static constexpr uint32_t level_bit(const utility::log_level lv) {
                return 1u << static_cast<uint32_t>(lv);
            }

        private:
            const record_storage& _storage;

            ImGuiTextFilter _text_filter;
            uint32_t _shown_level_mask{ make_level_mask(kDefaultMinLevel) }; // bit k = level k is shown

            // Seqs of the records that pass, ascending, so the ones of dropped records 
            // (below `first_seq()`) are always at the front and come off by popping from there.
            std::deque<record_seq_id_t> _seqs;

            // Boundary between examined and unexamined records.
            // every seq from here on is still unexamined, so only those need examining.
            record_seq_id_t _scanned_end_seq{ 0 };

            // Whether a filter changed since the last `refresh()`, which then examines every record again.
            bool _flag_dirty{ true };
        }; // class filtered_record_view

    } // namespace

    struct log_window::impl_t
    {
        // --- cross-thread handoff ---
        // NOTE: Held through a `shared_ptr` so that a record in flight stays safe while the window dies.
        std::shared_ptr<pending_record_queue> pending_record_q{ std::make_shared<pending_record_queue>() };

        // NOTE: drops the engine subscription on destruction
        utility::logger::subscription engine_log_subscription;

        // --- records ---
        record_storage records;
        filtered_record_view filtered_view{ records }; // NOTE: reads `records`, so declared after it

        // --- view options ---
        bool flag_autoscroll{ true }; // keep scrolling if already at the bottom

    public:
        impl_t() = default;

        void clear_records()
        {
            pending_record_q->clear();
            records.clear();
        }

        void set_record_capacity(const size_t capacity)
        {
            const size_t effective_capacity = std::clamp(capacity, kMinLogCapacity, kMaxLogCapacity);
            // Keeps the pending queue on the same capacity, so neither side outgrows the other.
            pending_record_q->set_capacity(effective_capacity);
            records.set_capacity(effective_capacity);
        }

        void drain_pending_records()
        {
            std::deque<record_t> pending_records = pending_record_q->dequeue_all();
            if (pending_records.empty()) { return; }

            records.append(std::move(pending_records));
        }

        // `do_copy` is set when clipboard copy button was pressed.
        void draw_toolbar(bool& do_copy)
        {
            if (ImGui::BeginPopup("Options")) {
                ImGui::Checkbox("Auto-scroll", &flag_autoscroll);

                int capacity = static_cast<int>(records.capacity());
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
                const size_t level_count = records.level_count(lv);
                bool shown = filtered_view.is_level_shown(lv);

                ImGui::SameLine();
                const std::string label = utility::string::c_format("%s %zu###lv%zu", level_tag(lv), level_count, i);
                if (ImGui::Checkbox(label.c_str(), &shown)) {
                    filtered_view.set_level_shown(lv, shown);
                }
                ImGui::SetItemTooltip("%s: %zu record(s) buffered", level_tag(lv), level_count);
            }

            ImGui::SameLine();
            filtered_view.draw_text_filter("Filter", -100.0f);
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
                [pending_record_q = _impl->pending_record_q](const utility::log_level lv, const std::string& msg)
            {
                pending_record_q->enqueue(lv, msg);
            });
        }
    }

    log_window::~log_window() = default;

    void log_window::add_log(
        const utility::log_level lv,
        const std::string_view sv)
    {
        _impl->pending_record_q->enqueue(lv, sv);
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
            filtered_record_view& filtered_view = _impl->filtered_view;
            filtered_view.refresh();

            const size_t visible_record_count = filtered_view.size();

            // Copy takes the whole filtered view, not just the rows the clipper drew.
            if (do_copy)
            {
                std::string all_text;
                for (size_t i = 0; i < visible_record_count; ++i) {
                    all_text += filtered_view.at(i).text;
                    all_text += '\n';
                }
                ImGui::SetClipboardText(all_text.c_str());
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(visible_record_count));
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const record_t& record = filtered_view.at(static_cast<size_t>(i));
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
