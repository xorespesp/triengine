# An INTERFACE library has no source files (it produces no object files),
# so there is no need to add a source list. (headers can be omitted)
# Its purpose is to propagate "usage requirements" (include directories, link dependencies, etc.) to consuming targets.

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