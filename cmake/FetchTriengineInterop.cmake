if (NOT TARGET triengine_interop)
    message(STATUS "Fetching triengine_interop...")

    FetchContent_Declare(
        triengine_interop
        GIT_REPOSITORY https://github.com/xorespesp/triengine_interop.git
        GIT_TAG 6a1d2ea98652ebd2cf669ad6d92b8304589f05ea
        GIT_PROGRESS TRUE
        #SOURCE_DIR "D:/GitProjects/triengine/triengine_interop"
        EXCLUDE_FROM_ALL # Added in CMake 3.28
    )

    FetchContent_MakeAvailable(triengine_interop)
endif()
