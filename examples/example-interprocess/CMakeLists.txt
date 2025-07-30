set(TARGET_NAME example-interprocess)
set(ENABLE_OPENCV OFF)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/ipc_proto.hh"
        "src/ipc_service.cc"
        "src/ipc_service.hh"
        "src/task_dispatcher.hh"
        "src/renderer_process.cc"
        "src/renderer_process.hh"
        "src/viewer_process.cc"
        "src/viewer_process.hh"
        "src/example_interprocess_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

##################################################################
# See: https://stackoverflow.com/q/72913306

message(STATUS "Fetching boost...")
set(BOOST_INCLUDE_LIBRARIES interprocess date_time program_options)
set(BOOST_ENABLE_CMAKE ON)
set(FETCHCONTENT_QUIET FALSE) # Needed to print downloading progress
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.7z # downloading a zip release speeds up the download
    USES_TERMINAL_DOWNLOAD TRUE 
    GIT_PROGRESS TRUE   
    DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(Boost)

##################################################################

target_link_libraries(${TARGET_NAME} PRIVATE 
    Triengine::triengine
    Boost::interprocess
    Boost::date_time
    Boost::program_options
    example-common)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")