#pragma once
// Implementation of shader_storage_vector.hh; include that header instead.

#include <triengine/utility/debug_utils.hh>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace triengine::core::shader_block
{
    template <class _Element>
    shader_storage_vector<_Element>::shader_storage_vector(const uint32_t binding_point_index)
        : _binding_point_index{ binding_point_index }
        , _bytes(kInitialCapacity * kStride, std::byte{ 0 })
    {
        _buffer_id = detail::create_buffer(_bytes);
        _buffer_capacity = kInitialCapacity;
    }

    template <class _Element>
    shader_storage_vector<_Element>::~shader_storage_vector()
    {
        if (_buffer_id != 0) {
            detail::delete_buffer(_buffer_id);
        }
    }

    template <class _Element>
    shader_storage_vector<_Element>::shader_storage_vector(shader_storage_vector&& rhs) noexcept
        : _buffer_id{ std::exchange(rhs._buffer_id, 0u) }
        , _binding_point_index{ rhs._binding_point_index }
        , _buffer_capacity{ std::exchange(rhs._buffer_capacity, size_t{ 0 }) }
        , _size{ std::exchange(rhs._size, size_t{ 0 }) }
        , _bytes{ std::move(rhs._bytes) }
        , _dirty_begin{ std::exchange(rhs._dirty_begin, size_t{ 0 }) }
        , _dirty_end{ std::exchange(rhs._dirty_end, size_t{ 0 }) }
    {
    }

    template <class _Element>
    shader_storage_vector<_Element>& shader_storage_vector<_Element>::operator=(shader_storage_vector&& rhs) noexcept
    {
        if (this != &rhs) {
            if (_buffer_id != 0) {
                detail::delete_buffer(_buffer_id);
            }
            _buffer_id = std::exchange(rhs._buffer_id, 0u);
            _binding_point_index = rhs._binding_point_index;
            _buffer_capacity = std::exchange(rhs._buffer_capacity, size_t{ 0 });
            _size = std::exchange(rhs._size, size_t{ 0 });
            _bytes = std::move(rhs._bytes);
            _dirty_begin = std::exchange(rhs._dirty_begin, size_t{ 0 });
            _dirty_end = std::exchange(rhs._dirty_end, size_t{ 0 });
        }
        return *this;
    }

    template <class _Element>
    size_t shader_storage_vector<_Element>::push_back(const _Element& element)
    {
        this->_ensure_buffer_id();
        if (_size == this->capacity()) {
            this->_grow(std::max(this->capacity() * 2, kInitialCapacity));
        }
        const size_t index = _size++;
        this->set(index, element);
        return index;
    }

    template <class _Element>
    void shader_storage_vector<_Element>::set(const size_t index, const _Element& element)
    {
        this->_ensure_buffer_id();
        if (index >= _size) {
            TRIENGINE_PANIC("Storage vector element %zu is outside its %zu elements", index, _size);
        }

        std::array<std::byte, kStride> serialized; // `serialize_element` writes every byte
        layout::serialize_element(0, element, serialized.data(), serialized.size());

        std::byte* const stored = _bytes.data() + index * kStride;
        if (std::memcmp(stored, serialized.data(), kStride) == 0) {
            return;
        }
        std::memcpy(stored, serialized.data(), kStride);

        const bool any_dirty = _dirty_begin != _dirty_end;
        _dirty_begin = any_dirty ? std::min(_dirty_begin, index) : index;
        _dirty_end = any_dirty ? std::max(_dirty_end, index + 1) : index + 1;
    }

    template <class _Element>
    void shader_storage_vector<_Element>::clear()
    {
        this->_ensure_buffer_id();
        // A write that was never uploaded means a draw read an old element.
        TRIENGINE_ASSERT(_dirty_begin == _dirty_end);
        _size = 0;
    }

    template <class _Element>
    void shader_storage_vector<_Element>::reserve(const size_t capacity)
    {
        this->_ensure_buffer_id();
        if (capacity > this->capacity()) {
            this->_grow(capacity);
        }
    }

    template <class _Element>
    void shader_storage_vector<_Element>::_grow(const size_t capacity)
    {
        if (capacity > std::numeric_limits<size_t>::max() / kStride) {
            TRIENGINE_PANIC("%zu storage vector elements do not fit in a byte count", capacity);
        }
        // Perform CPU copy only; `upload` replaces the buffer, once however many times the copy grew.
        _bytes.resize(capacity * kStride, std::byte{ 0 });
    }

    template <class _Element>
    void shader_storage_vector<_Element>::upload()
    {
        this->_ensure_buffer_id();

        if (_buffer_capacity < this->capacity()) {
            // A new buffer created with the whole copy. A GPU copy of the old buffer would send fewer bytes, but 
            // on NVIDIA the first CPU write into a buffer only the GPU has written can stall as long as the buffer is large.
            const uint32_t buffer_id = detail::create_buffer(_bytes);
            detail::delete_buffer(_buffer_id);
            _buffer_id = buffer_id;
            _buffer_capacity = this->capacity();
            detail::bind_buffer(type(), _binding_point_index, _buffer_id);
            _dirty_begin = _dirty_end = 0;
            return;
        }

        if (_dirty_begin == _dirty_end) {
            return;
        }
        const size_t offset = _dirty_begin * kStride;
        detail::upload_bytes(_buffer_id, offset, _bytes.data() + offset, (_dirty_end - _dirty_begin) * kStride);
        _dirty_begin = _dirty_end = 0;
    }

    template <class _Element>
    void shader_storage_vector<_Element>::bind() const
    {
        this->_ensure_buffer_id();
        detail::bind_buffer(type(), _binding_point_index, _buffer_id);
    }

    template <class _Element>
    void shader_storage_vector<_Element>::_ensure_buffer_id() const
    {
        if (_buffer_id == 0) {
            TRIENGINE_PANIC("Invalid buffer id (binding point %u)", _binding_point_index);
        }
    }

} // namespace
