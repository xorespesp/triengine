if (NOT TARGET triengine_interop)
    message(STATUS "Fetching triengine_interop...")

    FetchContent_Declare(
        triengine_interop
        GIT_REPOSITORY https://github.com/xorespesp/triengine_interop.git
        GIT_TAG 50d3b59d7445f5de4a06ab7fac494f657167f022
        GIT_PROGRESS TRUE
        #SOURCE_DIR "D:/GitProjects/triengine/triengine_interop"
        EXCLUDE_FROM_ALL # Added in CMake 3.28
    )

    FetchContent_MakeAvailable(triengine_interop)
endif()
