#include <triengine/texture.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/extern/stb_image.h>

#include <fstream>
#include <mutex>
#include <optional>
#include <array>

namespace triengine
{
    namespace {

        bool _try_map_image_format_to_texture_format(
            const image_format_type image_format,
            const bool apply_gamma_correction,
            GLenum& texture_internal_format/* out */,
            GLenum& texture_format/* out */)
        {
            switch (image_format) {
            case image_format_type::greyscale:
                // Internal format GL_R8 (OpenGL 3.0+).
                // For older OpenGL, consider using GL_RED or GL_LUMINANCE.
                texture_internal_format = GL_R8;
                texture_format = GL_RED;
                return true;
            case image_format_type::rg:
                texture_internal_format = GL_RG8;
                texture_format = GL_RG;
                return true;
            case image_format_type::rgb:
                // Using GL_RGB8 as a recommended 8-bit internal format.
                // GL_RGB is the "classic" but for modern usage, GL_RGB8 is more explicit.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8 : GL_RGB8;
                texture_format = GL_RGB;
                return true;
            case image_format_type::bgr:
                // Note: GL_BGR is not a valid internal format, just a transfer format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8 : GL_RGB8;
                texture_format = GL_BGR;
                return true;
            case image_format_type::rgba:
                // Using GL_RGBA8 as an 8-bit internal format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                texture_format = GL_RGBA;
                return true;
            case image_format_type::bgra:
                // Note: GL_BGRA is not a valid internal format, just a transfer format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                texture_format = GL_BGRA;
                return true;
            default:
                return false;
            }
        }

        class scoped_gl_pixel_store_guard
            : utility::noncopyable
        {
        public:
            scoped_gl_pixel_store_guard(GLenum pname, GLint new_value)
                : _pname{ pname }
            {
                ::glGetIntegerv(_pname, &_prev_value);
                if (_prev_value != new_value) {
                    ::glPixelStorei(_pname, new_value);
                }
            }

            ~scoped_gl_pixel_store_guard()
            {
                ::glPixelStorei(_pname, _prev_value);
            }

        private:
            GLenum _pname{};
            GLint _prev_value{};
        };

    } // namespace

    texture_2d::~texture_2d()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    texture_2d::texture_2d(texture_2d&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    texture_2d& texture_2d::operator=(texture_2d&& rhs) noexcept
    {
        if (this != &rhs) {
            if (this->is_valid()) {
                this->destroy();
            }
            std::swap(_texture_id, rhs._texture_id);
            std::swap(_image_format, rhs._image_format);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            std::swap(_params, rhs._params);
        }

        return *this;
    }

    bool texture_2d::is_valid() const noexcept {
        return _texture_id != kInvalidTextureID;
    }

    void texture_2d::create_from_memory(
        const uint8_t* const image_buffer,
        const image_format_type image_format, 
        const bool gamma_correction,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const texture_params_t& params,
        const bool generate_mipmap)
    {
        if (!image_buffer) {
            TRIENGINE_PANIC("invalid image buffer");
        }

        if (!width_pixels || !height_pixels) {
            TRIENGINE_PANIC("invalid image size");
        }

        GLenum internal_format{}, format{};
        if (!_try_map_image_format_to_texture_format(
            image_format,
            gamma_correction,
            internal_format,
            format))
        {
            TRIENGINE_PANIC("unsupported image format.");
        }

        // Create & Bind Texture
        GLuint new_tex_id{ kInvalidTextureID };
        GLCall(::glCreateTextures(GL_TEXTURE_2D, 1, &new_tex_id));

        // If user wants mipmaps but min_filter is not a mipmap-based filter, adjust it automatically
        GLint adjusted_min_filter{ params.min_filter };
        if (generate_mipmap && (params.min_filter == GL_LINEAR || params.min_filter == GL_NEAREST)) {
            TRIENGINE_WARN("Override texture min filter options to GL_LINEAR_MIPMAP_LINEAR");
            adjusted_min_filter = GL_LINEAR_MIPMAP_LINEAR;
        }

        // Set texture parameters (wrap, filter) for display
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_S, params.wrap_s));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_T, params.wrap_t));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_MIN_FILTER, adjusted_min_filter));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_MAG_FILTER, params.mag_filter));

        // Allocate (immutable) GPU storage for the texture
        // Ref: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
        const GLsizei num_mipmap_levels = generate_mipmap 
            ? static_cast<GLsizei>(std::floor(std::log2f(static_cast<float>(std::max(width_pixels, height_pixels))))) + 1
            : static_cast<GLsizei>(1); // 1 means no mipmaps
        GLCall(::glTextureStorage2D(
            new_tex_id,                         /* GLuint texture */
            num_mipmap_levels,                  /* GLsizei levels */
            internal_format,                    /* GLenum internalformat */
            static_cast<GLsizei>(width_pixels), /* GLsizei width */
            static_cast<GLsizei>(height_pixels) /* GLsizei height */
        ));

        {
            // Temporarily set pixel unpack alignment to 1 to avoid issues with image buffer alignment
            scoped_gl_pixel_store_guard unpack_alignment_guard{ GL_UNPACK_ALIGNMENT, 1 };

            // Upload the image data to the texture GPU storage
            GLCall(::glTextureSubImage2D(
                new_tex_id,                          /* GLuint texture */
                0,                                   /* GLint level */
                0,                                   /* GLint xoffset */
                0,                                   /* GLint yoffset */
                static_cast<GLsizei>(width_pixels),  /* GLsizei width */
                static_cast<GLsizei>(height_pixels), /* GLsizei height */
                format,                              /* GLenum format */
                GL_UNSIGNED_BYTE,                    /* GLenum type */
                image_buffer                         /* const void* pixels */
            ));
        }

        // Generate mipmaps if requested
        if (generate_mipmap) {
            GLCall(::glGenerateTextureMipmap(new_tex_id));
        }

        if (this->is_valid()) {
            this->destroy();
        }

        _texture_id = new_tex_id;
        _image_format = image_format;
        _width_pixels = width_pixels;
        _height_pixels = height_pixels;
    }

    void texture_2d::create_from_file(
        const std::filesystem::path& image_path,
        const bool gamma_correction,
        const texture_params_t& params,
        const bool generate_mipmap,
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

        // NOTE: `stbi_set_flip_vertically_on_load` is thread-unsafe
        // TODO: Use a mutex to protect this if multithreaded loading is needed
        // but for now, we assume single-threaded loading(OpenGL context is usually bound to a single thread anyway), so no mutex is needed.
        ::stbi_set_flip_vertically_on_load(flip_image);

        int32_t width_pixels{}, height_pixels{}, num_channels{};
        std::unique_ptr<stbi_uc, decltype(&::stbi_image_free)> image_data{
            ::stbi_load_from_memory(
                file_content.data(),
                static_cast<int32_t>(file_content.size()),
                &width_pixels,
                &height_pixels,
                &num_channels,
                0 // desired_channels (0 = auto)
            ),
            ::stbi_image_free
        };

        if (!image_data) {
            TRIENGINE_PANIC("failed to load texture file: %s", image_path.generic_u8string().c_str());
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

        const auto image_format = try_map_stbi_num_channels_to_image_format(num_channels);
        if (image_format == image_format_type::invalid) {
            TRIENGINE_PANIC("unsupported image format in file: %s", image_path.generic_u8string().c_str());
        }

        this->create_from_memory(
            image_data.get(),
            image_format,
            gamma_correction,
            width_pixels,
            height_pixels,
            params,
            generate_mipmap
        );
    }

    void texture_2d::create_from_uniform_color(
        const color3_f32& color)
    {
        // Convert float color to 0-255 range
        const std::array<uint8_t, 3> pixel_buff{
            static_cast<uint8_t>(std::clamp(color.r() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.g() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.b() * 255.0f, 0.0f, 255.0f))
        };

        this->create_from_memory(
            pixel_buff.data(),
            image_format_type::rgb,
            false,
            1,
            1,
            texture_params_t{},
            false
        );
    }

    void texture_2d::destroy() noexcept
    {
        if (this->is_valid()) {
            ::glDeleteTextures(1, &_texture_id);
        }
        _texture_id = kInvalidTextureID;
        _image_format = image_format_type::invalid;
        _width_pixels = _height_pixels = 0;
    }

    std::string texture_2d::dump() const
    {
        return utility::string::c_format(""
            "texture id: 0x%X\n"
            "image format: %d\n"
            "image size: %dx%d\n"
            , _texture_id
            , static_cast<GLenum>(_image_format)
            , _width_pixels
            , _height_pixels
        );
    }

} // namespace