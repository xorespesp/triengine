#include "image_buffer.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/extern/stb_image.h>

#include <fstream>
#include <mutex>

namespace triengine
{
    image_buffer::image_buffer(image_buffer&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    image_buffer& image_buffer::operator=(image_buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            _format = rhs._format;
            _width_pixels = rhs._width_pixels;
            _height_pixels = rhs._height_pixels;
            _stride_bytes = rhs._stride_bytes;
            _buffer = std::move(rhs._buffer);
            rhs.clear();
        }
        return *this;
    }

    image_buffer& image_buffer::prepare(
        const int32_t width, 
        const int32_t height, 
        const image_format_type format)
    {
        if (format == image_format_type::invalid) {
            TRIENGINE_PANIC("invalid image format.");
        }

        if (width <= 0 || height <= 0) {
            TRIENGINE_PANIC("invalid image dimensions.");
        }

        const uint32_t channel_size = get_image_format_channel_size(format);
        TRIENGINE_ASSERT(channel_size > 0);

        _format = format;
        _width_pixels = width;
        _height_pixels = height;
        _stride_bytes = static_cast<uint32_t>(static_cast<size_t>(width) * static_cast<size_t>(channel_size));
        _buffer.resize(static_cast<size_t>(_height_pixels) * static_cast<size_t>(_stride_bytes));

        return *this;
    }

    void image_buffer::clear() noexcept
    {
        _format = image_format_type::invalid;
        _width_pixels = 0;
        _height_pixels = 0;
        _stride_bytes = 0;
        _buffer.clear();
    }

    void image_buffer::copy_to(image_buffer& other) const
    {
        other._format = this->_format;
        other._width_pixels = this->_width_pixels;
        other._height_pixels = this->_height_pixels;
        other._stride_bytes = this->_stride_bytes;
        other._buffer = this->_buffer;
    }

    void image_buffer::load_from_memory(
        const uint8_t* const image_file_buff, 
        const size_t image_file_buff_size,
        const bool flip_image)
    {
        this->clear();
        
        if (!image_file_buff || image_file_buff_size == 0) {
            TRIENGINE_PANIC("invalid image file buffer");
        }

        // NOTE: `stbi_set_flip_vertically_on_load` is thread-unsafe
        // TODO: Use a mutex to protect this if multithreaded loading is needed
        // but for now, we assume single-threaded loading(OpenGL context is usually bound to a single thread anyway), so no mutex is needed.
        ::stbi_set_flip_vertically_on_load(flip_image);

        int32_t width_pixels{}, height_pixels{}, num_channels{};
        std::unique_ptr<stbi_uc, decltype(&::stbi_image_free)> image_data{
            ::stbi_load_from_memory(
                image_file_buff,
                static_cast<int32_t>(image_file_buff_size),
                &width_pixels,
                &height_pixels,
                &num_channels,
                0 // desired_channels (0 = auto)
            ),
            ::stbi_image_free
        };
        if (!image_data) {
            TRIENGINE_PANIC("failed to load image from memory buffer.");
        }

        const auto try_map_stbi_num_channels_to_image_format = 
            [](const int32_t stbi_num_channels) -> image_format_type {
                switch (stbi_num_channels) {
                case 1:  return image_format_type::greyscale;
                case 2:  return image_format_type::rg;
                // NOTE: `stbi_load` always converts image format to rgb or rgba internally when loading.
                case 3:  return image_format_type::rgb;
                case 4:  return image_format_type::rgba;
                default: return image_format_type::invalid;
                }
            };

        const image_format_type image_format = try_map_stbi_num_channels_to_image_format(num_channels);
        if (image_format == image_format_type::invalid) {
            TRIENGINE_PANIC("unsupported image format in memory buffer.");
        }
        
        const uint32_t stride_bytes = static_cast<uint32_t>(width_pixels) * get_image_format_channel_size(image_format);

        _width_pixels = width_pixels;
        _height_pixels = height_pixels;
        _format = image_format;
        _stride_bytes = stride_bytes;
        _buffer.resize(static_cast<size_t>(_height_pixels) * static_cast<size_t>(_stride_bytes));
        std::memcpy(_buffer.data(), image_data.get(), _buffer.size());
    }

    void image_buffer::load_from_file(
        const std::filesystem::path& image_path, 
        const bool flip_image)
    {
        if (!std::filesystem::exists(image_path) || !std::filesystem::is_regular_file(image_path)) {
            TRIENGINE_PANIC("invalid texture file path: %s", image_path.generic_u8string().c_str());
        }

        // [NOTE]
        // stb library provides an API to read image data from files, but to avoid encoding issues with file paths, 
        // it is safest and most cross-platform compatible to read the file contents directly into memory 
        // using standard STL and then use `stbi_load_from_memory`.

        std::vector<uint8_t> file_content;
        {
            // open file in binary mode and move the file pointer to the end(`std::ios::ate`) for file size measurement
            std::ifstream file{ image_path, std::ios::binary | std::ios::ate };
            if (!file.is_open()) {
                TRIENGINE_PANIC("failed to open texture file: %s", image_path.generic_u8string().c_str());
            }

            const std::streamsize file_size = file.tellg();
            if (file_size <= 0) {
                TRIENGINE_PANIC("texture file is empty: %s", image_path.generic_u8string().c_str());
            }

            file.seekg(0, std::ios::beg); // Go back to the beginning

            try {
                file_content.resize(static_cast<size_t>(file_size));
            } catch (const std::bad_alloc&) {
                TRIENGINE_PANIC("failed to allocate memory for texture file loading.");
            }

            if (!file.read(reinterpret_cast<char*>(file_content.data()), file_size)) {
                TRIENGINE_PANIC("failed to read texture file data: %s", image_path.generic_u8string().c_str());
            }
        }

        this->load_from_memory(
            file_content.data(), 
            file_content.size(), 
            flip_image
        );
    }

} // namespace