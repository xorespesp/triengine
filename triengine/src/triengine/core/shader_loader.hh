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
         * @brief 임베드된 바이너리 데이터로 로더 초기화
         * @param data 바이너리에 임베드된 zip 바이너리 데이터의 포인터
         * @param size 데이터의 크기 (바이트)
         * @return 초기화 성공 시 true, 실패 시 false
         */
        bool initialize(const unsigned char* data, size_t size);

        /**
         * @brief 아카이브 내 경로를 이용해 에셋 데이터를 가져옴
         * @param path_in_archive 아카이브 내의 전체 경로 (e.g: "shaders/pbr.frag")
         * @return 성공 시 에셋 데이터를 담은 std::string, 실패 시 std::nullopt
         */
        std::optional<std::string> load(const std::string& path_in_archive) const;

    private:
        struct impl;
        std::unique_ptr<impl> _impl;
    };

} // namespace