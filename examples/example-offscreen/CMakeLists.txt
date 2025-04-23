set(TARGET_NAME example-offscreen)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/example_offscreen_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

include(FetchOpenCV)

target_link_libraries(${TARGET_NAME} PRIVATE 
    Triengine::triengine
    example-common
    opencv_highgui
    opencv_imgcodecs
    opencv_imgproc
    opencv_core)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")