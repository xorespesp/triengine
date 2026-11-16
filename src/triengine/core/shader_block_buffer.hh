#pragma once
#include <triengine/core/shader_block_layout.hh>
#include <triengine/utility/noncopyable.hh>

#include <cstddef>
#include <cstdint>
#include <vector>

// GPU buffer layers of GLSL blocks(UBO/SSBO)
namespace triengine::core::shader_block
{
    // Owns the buffer of a block struct (`TRIENGINE_GLSL_BLOCK`), uploads the struct into it, 
    // and binds it to the binding point the GLSL block reads from.
    // NOTE: Create, use and destroy it on the thread where the GL context is current.
    //
    //   uniform_buffer<frame_block> frame_ubo{ 0 };   // layout(std140, binding = 0) uniform FrameBlock { ... };
    //   frame_ubo.update(frame);                      // each frame
    //   frame_ubo.bind();
    template <block_type _Type, layout_rule _Rule, class _Block>
    class block_buffer final
        : utility::noncopyable
    {
        static_assert(is_glsl_block_v<_Block>, "a block buffer needs a struct declared with TRIENGINE_GLSL_BLOCK");
        static_assert(_Type == block_type::shader_storage || _Rule == layout_rule::std140, "uniform blocks are std140");

    public:
        using layout = block_layout<_Rule, _Block>;

        static_assert(layout::runtime_array_stride() == 0, "block buffers hold fixed-size blocks only");

        static constexpr block_type type() noexcept { return _Type; }

        // Allocates the buffer, filled with zeros.
        explicit block_buffer(uint32_t binding_point_index);
        ~block_buffer();

        block_buffer(block_buffer&& rhs) noexcept;
        block_buffer& operator=(block_buffer&& rhs) noexcept;

        // Serializes `block` and uploads the bytes from the first to the last one that changed since the last update.
        // Uploads nothing when none did.
        void update(const _Block& block);

        void bind() const;

        uint32_t buffer_id() const noexcept { return _buffer_id; }
        uint32_t binding_point_index() const noexcept { return _binding_point_index; }

    private:
        void _ensure_buffer_id() const;

    private:
        uint32_t _buffer_id{ 0 };
        uint32_t _binding_point_index{ 0 };
        std::vector<std::byte> _staging_bytes;  // serialized by `update`, before the upload
        std::vector<std::byte> _uploaded_bytes; // what the buffer holds
    };

    template <class _Block>
    using uniform_buffer = block_buffer<block_type::uniform, layout_rule::std140, _Block>;

    template <class _Block, layout_rule _Rule = layout_rule::std430>
    using storage_buffer = block_buffer<block_type::shader_storage, _Rule, _Block>;
}

#include "shader_block_buffer_impl.hh"
