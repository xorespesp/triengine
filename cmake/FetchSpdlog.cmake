include_guard(GLOBAL)

if (NOT TARGET spdlog::spdlog)
    message(STATUS "Fetching spdlog...")

    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    set(SPDLOG_WCHAR_SUPPORT ON CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(spdlog)
endif()