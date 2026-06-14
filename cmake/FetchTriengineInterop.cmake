if (NOT TARGET triengine_interop)
    message(STATUS "Fetching triengine_interop...")

    FetchContent_Declare(
        triengine_interop
        GIT_REPOSITORY https://github.com/xorespesp/triengine_interop.git
        GIT_TAG 00595da07f93d24d571d02095226739afb687c29
        GIT_PROGRESS TRUE
        #SOURCE_DIR "D:/GitProjects/triengine/triengine_interop"
        EXCLUDE_FROM_ALL # Added in CMake 3.28
    )

    FetchContent_MakeAvailable(triengine_interop)
endif()
