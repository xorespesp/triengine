# INTERFACE 라이브러리는 소스 파일을 가지지 않으므로(아무런 오브젝트 파일도 생성하지 않음)
# 소스 목록을 굳이 추가할 필요 없음 (헤더는 생략 가능)
# 자신을 사용하는 타겟에게 포함 경로(include directories)나 링크 의존성 같은 "사용 요구사항"을 전달하는 목적적

add_library(example-common INTERFACE)

target_include_directories(example-common
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

include(FetchSpdlog)
include(FetchMagicEnum)

target_link_libraries(example-common
    INTERFACE
        spdlog::spdlog
        magic_enum::magic_enum
)

target_compile_definitions(example-common
    INTERFACE
        SPDLOG_COMPILED_LIB
        SPDLOG_WCHAR_TO_UTF8_SUPPORT
)