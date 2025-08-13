#include "shader_loader.hh"
#include <unordered_map>
#include <triengine/core/shader_preprocessor.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/extern/miniz/miniz.h>

namespace triengine::core
{
    struct shader_loader::impl
    {
    private:
        struct zip_archive_handle_deleter{
            void operator()(mz_zip_archive* h) const {
                if (h) {
                    ::mz_zip_reader_end(h);
                    delete h;
                }
            }
        };

    public:
        impl() = default;
        ~impl() = default;

        bool initialize(
            const uint8_t* data,
            size_t size,
            std::string_view glsl_shader_version)
        {
            if (_is_initialized) {
                TRIENGINE_ERROR("shader_loader is already initialized.");
                return false;
            }

            _glsl_shader_version = std::string{ glsl_shader_version };

            if (auto new_handle = std::unique_ptr<mz_zip_archive, zip_archive_handle_deleter>(new mz_zip_archive{});
                ::mz_zip_reader_init_mem(new_handle.get(), data, size, 0)) {
                _zip_archive_handle = std::move(new_handle);
            } else {
                TRIENGINE_ERROR("Failed to initialize zip archive from memory.");
                return false;
            }

            _shader_prep = std::make_unique<basic_shader_preprocessor>(
                [this](
                    const std::string_view current_file_canonical_path,
                    const basic_shader_preprocessor::include_directive_type parsed_include_type,
                    const std::string_view parsed_include_path
                ) -> std::optional<basic_shader_preprocessor::resolved_include_info_t>
                {
                    if (parsed_include_type == basic_shader_preprocessor::include_directive_type::quotes) // Handle: `#include "path/to/file.glsl"`
                    {
                        std::string parsed_include_canonical_path = this->_resolve_include_path(
                            current_file_canonical_path,
                            parsed_include_path
                        );

                        if (parsed_include_canonical_path.empty()) {
                            TRIENGINE_ERROR("Failed to resolve include path '%.*s' from '%.*s'.",
                                static_cast<int>(parsed_include_path.length()), parsed_include_path.data(),
                                static_cast<int>(current_file_canonical_path.length()), current_file_canonical_path.data()
                            );
                            return std::nullopt;
                        }

                        size_t uncompressed_size = 0;
                        void* p_uncompressed_data = ::mz_zip_reader_extract_file_to_heap(
                            _zip_archive_handle.get(),
                            parsed_include_canonical_path.c_str(),
                            &uncompressed_size, 
                            0
                        );
                        if (!p_uncompressed_data) {
                            TRIENGINE_ERROR("Failed to extract file from archive: %s", current_file_canonical_path.data());
                            return std::nullopt;
                        }
                        std::string shader_file_content{ static_cast<const char*>(p_uncompressed_data), uncompressed_size };
                        ::mz_free(p_uncompressed_data);

                        return basic_shader_preprocessor::resolved_include_info_t{
                            std::move(parsed_include_canonical_path),
                            std::move(shader_file_content)
                        };
                    }
                    else
                    {
                        TRIENGINE_WARN("Unsupported include directive type: %d"
                            , static_cast<int>(parsed_include_type)
                        );

                        return std::nullopt;
                    }
                });

            TRIENGINE_TRACE("Zip archive initialized with %u entries.", _zip_archive_handle->m_total_files);
            _is_initialized = true;
            return true;
        }

        std::optional<std::string> load(const std::string& path_in_archive)
        {
            if (!_is_initialized) {
                TRIENGINE_ERROR("Attempted to access shader_loader before initialization.");
                return std::nullopt;
            }

            // Check if the shader is already cached
            if (auto cache_it = _shader_cache.find(path_in_archive); 
                cache_it != _shader_cache.end()) {
                return cache_it->second;
            }

            size_t uncompressed_size = 0;
            void* p_uncompressed_data = ::mz_zip_reader_extract_file_to_heap(
                _zip_archive_handle.get(),
                path_in_archive.c_str(), 
                &uncompressed_size, 
                0
            );

            if (!p_uncompressed_data) {
                TRIENGINE_ERROR("Failed to extract file from archive: %s", path_in_archive.c_str());
                return std::nullopt;
            }

            const auto p_deleter = [](void* p) { ::mz_free(p); };
            std::unique_ptr<void, decltype(p_deleter)> p_guard(p_uncompressed_data, p_deleter);

            auto preprocessed_shader_content = _shader_prep->process_from_memory(
                std::string_view{ static_cast<const char*>(p_uncompressed_data), uncompressed_size },
                path_in_archive
            );

            if (!preprocessed_shader_content.has_value()) {
                TRIENGINE_ERROR("Failed to preprocess shader content: %s", path_in_archive.c_str());
                return std::nullopt;
            }

            std::string loaded_shader_content = _glsl_shader_version + "\n" + preprocessed_shader_content.value();

            // store in cache
            _shader_cache[path_in_archive] = loaded_shader_content;
            return loaded_shader_content;
        }

        const std::string& get_glsl_shader_version() const noexcept {
            return _glsl_shader_version;
        }

    private:

        /**
         * @brief Resolves and normalizes a virtual include path (e.g., a path inside a zip archive).
         * @details This function processes '.' and '..' components to produce a canonical path.
         * e.g., "shaders/lighting/../common/utils.glsl" -> "shaders/common/utils.glsl"
         * @param base_file_canonical_path The canonical path of the file containing the include directive.
         * @param parsed_include_path The relative or absolute-like path parsed from the include directive.
         * @return The resulting normalized canonical path. Returns an empty string on failure (e.g., navigating above the root).
         */
        std::string _resolve_include_path(
            const std::string_view base_file_canonical_path,
            const std::string_view parsed_include_path)
        {
            std::string_view full_path_sv;
            std::string combined_path_buffer; // A temporary buffer used only when combining relative paths.

            // If the include path starts with '/', treat it as an 
            // absolute-like path from the root, ignoring the base path.
            if (!parsed_include_path.empty() && parsed_include_path.front() == '/')
            {
                full_path_sv = parsed_include_path.substr(1);
            }
            else
            {
                // Extract the directory part from the base path.
                const auto last_slash_pos = base_file_canonical_path.find_last_of('/');
                const std::string_view base_dir = (last_slash_pos != std::string_view::npos)
                                                  ? base_file_canonical_path.substr(0, last_slash_pos)
                                                  : "";
        
                // Combine the base directory and the relative include path into the buffer,
                // as string_views cannot be concatenated directly without an underlying buffer.
                combined_path_buffer.reserve(base_dir.length() + 1 + parsed_include_path.length());
                combined_path_buffer.append(base_dir);
                if (!base_dir.empty() && !parsed_include_path.empty()) {
                    combined_path_buffer.push_back('/');
                }
                combined_path_buffer.append(parsed_include_path);
                full_path_sv = combined_path_buffer;
            }

            std::vector<std::string_view> components;
            size_t start = 0;
            while (start < full_path_sv.length())
            {
                size_t end = full_path_sv.find('/', start);
                if (end == std::string_view::npos) {
                    end = full_path_sv.length();
                }
        
                std::string_view component = full_path_sv.substr(start, end - start);
                start = end + 1;

                if (component == "." || component.empty()) {
                    // Ignore the current directory ('.') or empty components (from '//').
                    continue;
                }

                if (component == "..") {
                    // Handle the parent directory ('..').
                    if (!components.empty()) {
                        components.pop_back();
                    } else {
                        // This is an attempt to navigate above the root of the virtual file system.
                        TRIENGINE_ERROR(
                            "Path resolution failed: attempted to navigate above root from '%.*s'",
                            static_cast<int>(full_path_sv.length()), full_path_sv.data()
                        );
                        return ""; // Return an empty string on failure.
                    }
                } else {
                    components.push_back(component);
                }
            } // while

            // Join the normalized components to form the final canonical path.
            if (components.empty()) {
                return "";
            }
    
            std::string canonical_path; {
                size_t reserve_size = components.size() - 1; // for slashes
                for(const auto& comp : components) { reserve_size += comp.length(); }
                canonical_path.reserve(reserve_size);
                for (size_t i = 0; i < components.size(); ++i) {
                    canonical_path.append(components[i]);
                    if (i < components.size() - 1) {
                        canonical_path.push_back('/');
                    }
                }
            }

            return canonical_path;
        }

    private:
        bool _is_initialized{ false };
        std::unique_ptr<mz_zip_archive, zip_archive_handle_deleter> _zip_archive_handle;
        std::string _glsl_shader_version;
        std::unique_ptr<basic_shader_preprocessor> _shader_prep;
        std::unordered_map<std::string, std::string> _shader_cache;
    }; // class impl

    shader_loader::shader_loader()
        : _impl{ std::make_unique<impl>() }
    { }

    shader_loader::~shader_loader() = default;
    shader_loader::shader_loader(shader_loader&&) noexcept = default;
    shader_loader& shader_loader::operator=(shader_loader&&) noexcept = default;

    bool shader_loader::initialize(
        const uint8_t* const data,
        const size_t size,
        const std::string_view glsl_shader_version)
    {
        return _impl->initialize(data, size, glsl_shader_version);
    }

    std::optional<std::string> shader_loader::load(const std::string& path_in_archive)
    {
        return _impl->load(path_in_archive);
    }

    const std::string& shader_loader::get_glsl_shader_version() const noexcept
    {
        return _impl->get_glsl_shader_version();
    }

} // namespace