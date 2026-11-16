#include "shader_block_buffer.hh"
#include <triengine/utility/gl_utils.hh>

#include <glad/gl.h>

#include <algorithm>

namespace triengine::core::shader_block::detail
{
    uint32_t create_buffer(const std::vector<std::byte>& initial_bytes)
    {
        GLuint buffer_id = 0;
        GLCall(::glCreateBuffers(1, &buffer_id));
        // Immutable storage: a block's size never changes, and a growing storage vector replaces its buffer.
        // `GL_DYNAMIC_STORAGE_BIT` allows `glNamedBufferSubData`.
        GLCall(::glNamedBufferStorage(buffer_id, static_cast<GLsizeiptr>(initial_bytes.size()), initial_bytes.data(), GL_DYNAMIC_STORAGE_BIT));
        return buffer_id;
    }

    void delete_buffer(const uint32_t buffer_id) noexcept
    {
        ::glDeleteBuffers(1, &buffer_id);
    }

    void upload_bytes(
        const uint32_t buffer_id,
        const size_t offset,
        const std::byte* const data,
        const size_t byte_count)
    {
        GLCall(::glNamedBufferSubData(buffer_id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(byte_count), data));
    }

    void upload_changed_bytes(
        const uint32_t buffer_id,
        const std::vector<std::byte>& new_bytes,
        const std::vector<std::byte>& old_bytes)
    {
        TRIENGINE_ASSERT(new_bytes.size() == old_bytes.size());

        const auto first = std::mismatch(new_bytes.begin(), new_bytes.end(), old_bytes.begin(), old_bytes.end()).first;
        if (first == new_bytes.end()) {
            return;
        }
        const auto last = std::mismatch(new_bytes.rbegin(), new_bytes.rend(), old_bytes.rbegin(), old_bytes.rend()).first;

        // One upload from the first to the last changed byte: fewer GL calls than one per changed run.
        const auto offset = first - new_bytes.begin();
        const auto byte_count = last.base() - first;
        upload_bytes(buffer_id, static_cast<size_t>(offset), new_bytes.data() + offset, static_cast<size_t>(byte_count));
    }

    void bind_buffer(const block_type type, const uint32_t binding_point_index, const uint32_t buffer_id)
    {
        const GLenum target = type == block_type::uniform ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
        GLCall(::glBindBufferBase(target, binding_point_index, buffer_id));
    }

} // namespace
