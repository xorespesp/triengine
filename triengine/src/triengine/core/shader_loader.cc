#include "shader_loader.hh"
#include <unordered_map>
#include <triengine/extern/miniz/miniz.h>
#include <triengine/core/shader_preprocessor.hh>
#include <triengine/utility/debug_utils.hh>

#include <triengine/shaders/includes/phong_lighting_shaders.h>
#include <triengine/shaders/includes/smaa_shaders.h>
#include <triengine/shaders/includes/hdr_shaders.h>

namespace triengine::core
{
    // See: https://stackoverflow.com/questions/13872544/gcc-stringification-and-inline-glsl
    static const char glslShaderVersion[] = "#version 450 core\n";

    struct shader_loader::impl
    {
        mutable mz_zip_archive zip_archive{};
        mutable std::unordered_map<std::string, std::string> cache;
        shader_preprocessor shader_prep;
        bool is_initialized{ false };

        impl() {
            shader_prep.register_system_include_from_memory("phong_lighting", shaders::includes::kPhongLightingShader);
            shader_prep.register_system_include_from_memory("hdr", shaders::includes::kHDRShader);
            shader_prep.register_system_include_from_memory("SMAA.hlsl", shaders::includes::kSMAAShaders);
        }
        
        ~impl() {
            if (is_initialized) {
                ::mz_zip_reader_end(&zip_archive);
            }
        }

        std::optional<std::string> load_impl(const std::string& path_in_archive) const
        {
            if (!is_initialized) {
                TRIENGINE_ERROR("Attempted to access shader_loader before initialization.");
                return std::nullopt;
            }

            // 캐시 확인
            auto it = cache.find(path_in_archive);
            if (it != cache.end()) {
                return it->second;
            }

            // 아카이브에서 압축 해제
            size_t uncompressed_size = 0;
            void* p_uncompressed_data = ::mz_zip_reader_extract_file_to_heap(&zip_archive, path_in_archive.c_str(), &uncompressed_size, 0);
            if (!p_uncompressed_data) {
                TRIENGINE_ERROR("Failed to extract file from archive: %s", path_in_archive.c_str());
                return std::nullopt;
            }

            std::string preprocessed_shader_content = shader_prep.process_from_memory(
                std::string_view{ static_cast<const char*>(p_uncompressed_data), uncompressed_size },
                path_in_archive
            );

            preprocessed_shader_content = glslShaderVersion + preprocessed_shader_content;

            ::mz_free(p_uncompressed_data);

            // 전처리한 결과를 캐시에 저장
            cache[path_in_archive] = preprocessed_shader_content;
            return preprocessed_shader_content;
        }
    };

    shader_loader::shader_loader()
        : _impl{ std::make_unique<impl>() }
    { }

    shader_loader::~shader_loader() = default;
    shader_loader::shader_loader(shader_loader&&) noexcept = default;
    shader_loader& shader_loader::operator=(shader_loader&&) noexcept = default;

    bool shader_loader::initialize(const unsigned char* data, size_t size)
    {
        if (_impl->is_initialized) {
            TRIENGINE_ERROR("shader_loader is already initialized.");
            return true;
        }

        if (!::mz_zip_reader_init_mem(&_impl->zip_archive, data, size, 0)) {
            TRIENGINE_ERROR("Failed to initialize zip archive from memory.");
            _impl->is_initialized = false;
            return false;
        }

        TRIENGINE_TRACE("Zip archive initialized with %u entries.", _impl->zip_archive.m_total_files);
        _impl->is_initialized = true;
        return true;
    }

    std::optional<std::string> shader_loader::load(const std::string& path_in_archive) const
    {
        return _impl->load_impl(path_in_archive);
    }

    std::string shader_loader::get_glsl_shader_version() const noexcept
    {
        return glslShaderVersion;
    }

} // namespace