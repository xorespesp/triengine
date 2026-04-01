if (NOT TARGET freetype)
    message(STATUS "Fetching freetype...")

    FetchContent_Declare(freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG        VER-2-14-1
        GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
    )

    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(freetype)
endif()