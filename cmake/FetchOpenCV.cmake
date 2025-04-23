message(STATUS "Fetching OpenCV...")

FetchContent_Declare(
    opencv 
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG        4.9.0
    GIT_SHALLOW TRUE # git clone --depth=1
    GIT_PROGRESS TRUE
)

# Build only specified OpenCV modules
set(BUILD_LIST "core,imgproc,imgcodecs,highgui" CACHE STRING "" FORCE)

set(BUILD_SHARED_LIBS         OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_world        OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES            OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS               OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS          OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS                OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_apps         OFF CACHE BOOL "" FORCE)

set(WITH_CUDA                 OFF CACHE BOOL "" FORCE)
set(WITH_IPP                  OFF CACHE BOOL "" FORCE)
set(WITH_FFMPEG               OFF CACHE BOOL "" FORCE)
set(WITH_PROTOBUF             OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(opencv)