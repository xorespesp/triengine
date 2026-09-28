#pragma once
#include <triengine/utility/debug_utils.hh>

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace triengine::utility
{
    // Fixed-capacity FIFO that overwrites the oldest entry when full.
    // Storage is allocated once up front and never reallocates.
    // Entries are constructed on insertion and destroyed by `clear()`.
    template <typename _Ty>
    class circular_buffer {
    private:
        template <typename _Buffer, typename _Value>
        class basic_iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = _Ty;
            using difference_type = std::ptrdiff_t;
            using pointer = _Value*;
            using reference = _Value&;

            basic_iterator() = default;
            basic_iterator(_Buffer* buffer, size_t index) noexcept : _buffer{ buffer }, _index{ index } {}

            reference operator*() const noexcept { return (*_buffer)[_index]; }
            pointer operator->() const noexcept { return &(*_buffer)[_index]; }
            basic_iterator& operator++() noexcept { ++_index; return *this; }
            basic_iterator operator++(int) noexcept { basic_iterator prev = *this; ++_index; return prev; }

            friend bool operator==(const basic_iterator& a, const basic_iterator& b) noexcept {
                return a._buffer == b._buffer && a._index == b._index;
            }
            friend bool operator!=(const basic_iterator& a, const basic_iterator& b) noexcept { return !(a == b); }

        private:
            _Buffer* _buffer{};
            size_t _index{}; // logical index
        };

    public:
        using value_type = _Ty;
        using size_type = size_t;
        using iterator = basic_iterator<circular_buffer, _Ty>;
        using const_iterator = basic_iterator<const circular_buffer, const _Ty>;

    public:
        circular_buffer() = default;
        explicit circular_buffer(size_type capacity) : _capacity{ capacity } { _storage.reserve(capacity); }

        circular_buffer(const circular_buffer& other)
            : _capacity{ other._capacity }
            , _head{ other._head }
        {
            // Copying a vector only reserves its size; reserve the full capacity so later pushes never reallocate.
            _storage.reserve(_capacity);
            _storage.insert(_storage.end(), other._storage.begin(), other._storage.end());
        }

        circular_buffer(circular_buffer&& other) noexcept
            : _storage{ std::move(other._storage) }
            , _capacity{ std::exchange(other._capacity, 0) }
            , _head{ std::exchange(other._head, 0) }
        {
            other._storage.clear();
        }

        circular_buffer& operator=(const circular_buffer& other) {
            if (this != &other) { *this = circular_buffer(other); }
            return *this;
        }

        circular_buffer& operator=(circular_buffer&& other) noexcept {
            if (this != &other) {
                _storage = std::move(other._storage);
                _capacity = std::exchange(other._capacity, 0);
                _head = std::exchange(other._head, 0);
                other._storage.clear();
            }
            return *this;
        }

        ~circular_buffer() = default;

        // Appends until full, then overwrites the oldest entry. Returns the inserted entry.
        template <typename... _Args>
        _Ty& emplace_back(_Args&&... args) {
            if (_capacity == 0) { TRIENGINE_PANIC("Cannot write to a zero-capacity circular buffer"); }
            if (_storage.size() < _capacity) {
                _storage.push_back(_Ty{ std::forward<_Args>(args)... });
                return _storage.back();
            }
            _Ty& slot = _storage[_head];
            slot = _Ty{ std::forward<_Args>(args)... };
            _head = (_head + 1) % _capacity;
            return slot;
        }

        void push_back(const _Ty& value) { this->emplace_back(value); }
        void push_back(_Ty&& value) { this->emplace_back(std::move(value)); }

        void clear() noexcept {
            _storage.clear();
            _head = 0;
        }

        size_type size() const noexcept { return _storage.size(); }
        size_type capacity() const noexcept { return _capacity; }
        bool empty() const noexcept { return _storage.empty(); }
        bool full() const noexcept { return _storage.size() == _capacity; }

        // Logical index zero refers to the oldest retained entry.
        _Ty& operator[](size_type index) noexcept { return _storage[this->_physical_index(index)]; }
        const _Ty& operator[](size_type index) const noexcept { return _storage[this->_physical_index(index)]; }

        _Ty& front() noexcept { return (*this)[0]; }
        const _Ty& front() const noexcept { return (*this)[0]; }
        _Ty& back() noexcept { return (*this)[size() - 1]; }
        const _Ty& back() const noexcept { return (*this)[size() - 1]; }

        // Oldest to newest.
        iterator begin() noexcept { return { this, 0 }; }
        iterator end() noexcept { return { this, this->size() }; }
        const_iterator begin() const noexcept { return { this, 0 }; }
        const_iterator end() const noexcept { return { this, this->size() }; }

        // Physical storage for APIs that take a base pointer and a wrap-around offset (e.g. ImPlot's `offset`):
        // `size()` entries start at `data()`, and the oldest one is at `head_index()`.
        _Ty* data() noexcept { return _storage.data(); }
        const _Ty* data() const noexcept { return _storage.data(); }
        size_type head_index() const noexcept { return _head; }

    private:
        size_type _physical_index(size_type index) const noexcept {
            TRIENGINE_ASSERT(index < _storage.size());
            // `_head` stays 0 until the buffer is full, so this is the identity before the first wrap.
            return (_head + index) % _storage.size();
        }

    private:
        std::vector<_Ty> _storage;
        size_type _capacity{};
        size_type _head{}; // physical index of the oldest entry
    };

} // namespace
