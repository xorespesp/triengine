if (NOT TARGET magic_enum::magic_enum)
    message(STATUS "Fetching magic-enum...")

    FetchContent_Declare(magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG        v0.9.7
        GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    set(MAGIC_ENUM_OPT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MAGIC_ENUM_OPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(magic_enum)
endif()