#pragma once
// Implementation of shader_block_buffer.hh; include that header instead.

#include <utility>

namespace triengine::core::shader_block
{
    namespace detail
    {
        // GL calls of `block_buffer` (shader_block_buffer.cc).
        uint32_t create_buffer(const std::vector<std::byte>& initial_bytes);
        void delete_buffer(uint32_t buffer_id) noexcept;
        void upload_changed_bytes(uint32_t buffer_id, const std::vector<std::byte>& new_bytes, const std::vector<std::byte>& old_bytes);
        void bind_buffer(block_type type, uint32_t binding_point_index, uint32_t buffer_id);
    } // namespace

    template <block_type _Type, layout_rule _Rule, class _Block>
    block_buffer<_Type, _Rule, _Block>::block_buffer(const uint32_t binding_point_index)
        : _binding_point_index{ binding_point_index }
        , _uploaded_bytes(layout::buffer_size_for(0), std::byte{ 0 })
    {
        _buffer_id = detail::create_buffer(_uploaded_bytes);
    }

    template <block_type _Type, layout_rule _Rule, class _Block>
    block_buffer<_Type, _Rule, _Block>::~block_buffer()
    {
        if (_buffer_id != 0) {
            detail::delete_buffer(_buffer_id);
        }
    }

    template <block_type _Type, layout_rule _Rule, class _Block>
    block_buffer<_Type, _Rule, _Block>::block_buffer(block_buffer&& rhs) noexcept
        : _buffer_id{ std::exchange(rhs._buffer_id, 0u) }
        , _binding_point_index{ rhs._binding_point_index }
        , _staging_bytes{ std::move(rhs._staging_bytes) }
        , _uploaded_bytes{ std::move(rhs._uploaded_bytes) }
    {
    }

    template <block_type _Type, layout_rule _Rule, class _Block>
    block_buffer<_Type, _Rule, _Block>& block_buffer<_Type, _Rule, _Block>::operator=(block_buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            if (_buffer_id != 0) {
                detail::delete_buffer(_buffer_id);
            }
            _buffer_id = std::exchange(rhs._buffer_id, 0u);
            _binding_point_index = rhs._binding_point_index;
            _staging_bytes = std::move(rhs._staging_bytes);
            _uploaded_bytes = std::move(rhs._uploaded_bytes);
        }
        return *this;
    }

    template <block_type _Type, layout_rule _Rule, class _Block>
    void block_buffer<_Type, _Rule, _Block>::update(const _Block& block)
    {
        TRIENGINE_ASSERT(_buffer_id != 0);
        layout::serialize(block, _staging_bytes);
        detail::upload_changed_bytes(_buffer_id, _staging_bytes, _uploaded_bytes);
        // The buffer now holds the staging bytes. A swap, not a copy: the next `serialize` rewrites them whole.
        std::swap(_staging_bytes, _uploaded_bytes);
    }

    template <block_type _Type, layout_rule _Rule, class _Block>
    void block_buffer<_Type, _Rule, _Block>::bind() const
    {
        TRIENGINE_ASSERT(_buffer_id != 0);
        detail::bind_buffer(_Type, _binding_point_index, _buffer_id);
    }

} // namespace
