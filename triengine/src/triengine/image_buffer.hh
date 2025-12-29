#pragma once
#include <triengine/image_format.hh>
#include <triengine/utility/noncopyable.hh>

#include <filesystem>
#include <vector>

namespace triengine
{
    class image_buffer : public utility::noncopyable
    {
    public:
        image_buffer() = default;
        ~image_buffer() = default;

        image_buffer(image_buffer&& rhs) noexcept;
        image_buffer& operator=(image_buffer&& rhs) noexcept;

        image_format_type format() const noexcept { return _format; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }
        uint32_t stride_bytes() const noexcept { return _stride_bytes; }

        bool empty() const noexcept { return _buffer.empty(); }

        const uint8_t* data() const noexcept { return _buffer.data(); }
        uint8_t* data() noexcept { return _buffer.data(); }
        size_t size() const noexcept { return _buffer.size(); }
        
        /// \brief Prepare Image properties and allocate Image buffer.
        image_buffer& prepare(int32_t width, int32_t height, image_format_type format);

        /// \brief Clear Image data and reset properties.
        void clear() noexcept;

        /// \brief Copy image data to another image buffer.
        void copy_to(image_buffer& other) const;

        /// \brief Load image from memory buffer.
        void load_from_memory(
            const uint8_t* image_file_buff,
            size_t image_file_buff_size,
            bool flip_image = true
        );

        /// \brief Load image from file.
        void load_from_file(
            const std::filesystem::path& image_path,
            bool flip_image = true
        );

    private:
        image_format_type _format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};
        uint32_t _stride_bytes{};
        std::vector<uint8_t> _buffer; // image data buffer
    }; // class

} // namespace