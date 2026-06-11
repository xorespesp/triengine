if (NOT TARGET fmt::fmt)
    message(STATUS "Fetching Fmt...")

    FetchContent_Declare(Fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.0.2
        GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    # Ensure FetchContent_MakeAvailable uses the modern behavior (avoids some warnings)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    FetchContent_MakeAvailable(Fmt)
endif()
