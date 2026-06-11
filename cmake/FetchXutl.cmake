if (NOT TARGET xutl)
    message(STATUS "Fetching Xutl...")

    FetchContent_Declare(
        xutl
        GIT_REPOSITORY https://github.com/xorespesp/xutl.git
        GIT_TAG 6fe1d374d950a66e23f94eaaf12502ef063629de
        #GIT_SHALLOW TRUE # git clone --depth=1
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL # Added in CMake 3.28
    )

    FetchContent_MakeAvailable(xutl)
endif()