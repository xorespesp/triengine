if (NOT TARGET triengine_interop)
    message(STATUS "Fetching triengine_interop...")

    FetchContent_Declare(
        triengine_interop
        GIT_REPOSITORY https://github.com/xorespesp/triengine_interop.git
        GIT_TAG 0fd7c39fe5b96a6d65fd6268a0a9e90ea0255e5d
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL # Added in CMake 3.28
    )

    FetchContent_MakeAvailable(triengine_interop)
endif()
