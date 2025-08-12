#pragma once
#include <string>
#include <optional>
#include <memory>

namespace triengine::core
{
    class shader_loader
    {
    public:
        shader_loader();
        ~shader_loader();

        shader_loader(const shader_loader&) = delete;
        shader_loader& operator=(const shader_loader&) = delete;
        shader_loader(shader_loader&&) noexcept;
        shader_loader& operator=(shader_loader&&) noexcept;

        /**
         * @brief Initializes the loader with embedded binary data
         * @param data Pointer to the zip binary data embedded in the binary
         * @param size Size of the data (in bytes)
         * @return true if initialization succeeds, false if it fails
         */
        bool initialize(const unsigned char* data, size_t size);

        /**
         * @brief Retrieves asset data using its path inside the archive
         * @param path_in_archive Full path within the archive (e.g., "shaders/pbr.frag")
         * @return std::string containing the asset data on success, or std::nullopt on failure
         */
        std::optional<std::string> load(const std::string& path_in_archive) const;

        std::string get_glsl_shader_version() const noexcept;

    private:
        struct impl;
        std::unique_ptr<impl> _impl;
    };

} // namespace