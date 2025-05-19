#pragma once
#include <cxlib/cxlib_defs.h>

#include <vector>

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
    // utility structure for realtime plot
    template <typename _Ty>
    class scrolling_buffer {
    public:
        using value_type = _Ty;

    public:
        int32_t max_size;
        int32_t offset;
        std::vector<value_type> data;

    public:
        explicit scrolling_buffer(int32_t max_size_ = 2000)
            : max_size{ max_size_ }
            , offset{ 0 }
        { data.reserve(max_size); }

        void add_value(const value_type& new_value) {
            if (data.size() < max_size) {
                data.push_back(new_value);
            } else {
                data[offset] = new_value;
                offset = (offset + 1) % max_size;
            }
        }

        template <typename... _Args>
        void emplace_value(_Args&&... args) {
            this->add_value(value_type{ std::forward<_Args>(args)... });
        }

        void clear() {
            if (data.size() > 0) {
                data.clear();
                offset = 0;
            }
        }

    }; // class

} // namespace
_CXLIB_NAMESPACE_END
