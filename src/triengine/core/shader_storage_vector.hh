#pragma once
#include <triengine/core/shader_block_buffer.hh>
#include <triengine/core/shader_block_layout.hh>
#include <triengine/utility/noncopyable.hh>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace triengine::core::shader_block
{
    // A GLSL runtime-sized array of `_Element` in a shader storage buffer, which the CPU writes and shaders read.
    // Written like a `std::vector`, declared in GLSL with `DECL_STORAGE_VECTOR` (shaders/includes/storage_vector.glsl).
    // NOTE: Create, use and destroy it on the thread where the GL context is current.
    //
    //   shader_storage_vector<particle> particles{ 3 };   // DECL_STORAGE_VECTOR(3, ParticleBlock, Particle, u_particles);
    //   particles.clear();                                // each frame
    //   const size_t index = particles.push_back(p);      // read as u_particles.elements[index]
    //   particles.upload();                               // before the draws that read it
    //   particles.bind();
    //
    // Writes go to a CPU copy of the buffer and are compared with it element by element. `upload` sends the elements
    // that changed, from the first to the last, in one call; `clear` keeps the copy, so writing the same elements
    // again uploads nothing. Shaders see `capacity()` elements.
    // `bind` it before the draws that read it; `upload` also rebinds it after growing the buffer.
    template <class _Element>
    class shader_storage_vector final
        : utility::noncopyable
    {
        struct elements_block;

    public:
        using value_type = _Element;
        static_assert(!is_glsl_block_v<value_type>, "an element is a GLSL type or a TRIENGINE_GLSL_STRUCT struct, not a block");

        // `layout::verify(program_id, type(), "BlockName", binding_point_index)` once after linking
        using layout = block_layout<layout_rule::std430, elements_block>;

    private:
        // The block `DECL_STORAGE_VECTOR` declares: `buffer BlockName { _Element elements[]; }`
        struct elements_block {
            std::vector<value_type> elements;
            TRIENGINE_GLSL_STRUCT(elements_block, elements)
        };

        static constexpr size_t kStride = layout::runtime_array_stride();
        static constexpr size_t kInitialCapacity = 1024;

    public:
        static constexpr block_type type() noexcept { return block_type::shader_storage; }

        // Allocates the buffer for `kInitialCapacity` elements, filled with zeros.
        explicit shader_storage_vector(uint32_t binding_point_index);
        ~shader_storage_vector();

        shader_storage_vector(shader_storage_vector&& rhs) noexcept;
        shader_storage_vector& operator=(shader_storage_vector&& rhs) noexcept;

        // Appends `element` and returns its index. A full vector doubles its capacity.
        size_t push_back(const value_type& element);

        // Overwrites element `index`, below `size()`.
        void set(size_t index, const value_type& element);

        // Sets the size to 0. The elements stay in the buffer and in the CPU copy.
        void clear();

        // Grows the capacity to at least `capacity` elements.
        void reserve(size_t capacity);

        size_t size() const noexcept { return _size; }
        size_t capacity() const noexcept { return _bytes.size() / kStride; }
        bool empty() const noexcept { return _size == 0; }

        // Uploads the elements written since the last upload. When the capacity outgrew the buffer, 
        // replaces the buffer with one holding the whole CPU copy and binds it to the binding point.
        void upload();

        void bind() const;

        uint32_t buffer_id() const noexcept { return _buffer_id; }
        uint32_t binding_point_index() const noexcept { return _binding_point_index; }

    private:
        void _grow(size_t capacity);
        void _ensure_buffer_id() const;

    private:
        uint32_t _buffer_id{ 0 };
        uint32_t _binding_point_index{ 0 };
        size_t _buffer_capacity{ 0 };  // elements the buffer holds
        size_t _size{ 0 };
        std::vector<std::byte> _bytes; // what the buffer holds after the next upload; `capacity()` elements
        size_t _dirty_begin{ 0 };      // elements [_dirty_begin, _dirty_end) changed since the last upload
        size_t _dirty_end{ 0 };
    };
}

#include "shader_storage_vector_impl.hh"
