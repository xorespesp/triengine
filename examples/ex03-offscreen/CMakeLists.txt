set(TARGET_NAME ex03-offscreen)
set(ENABLE_OPENCV OFF)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/tiny_image_viewer.hh"
        "src/tiny_image_viewer.cc"
        "src/ex03_offscreen_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

target_link_libraries(${TARGET_NAME} PRIVATE 
    Triengine::triengine
    xutl)

if (ENABLE_OPENCV)
    find_package(OpenCV REQUIRED)

    target_link_libraries(${TARGET_NAME} PRIVATE 
        opencv_highgui
        opencv_imgcodecs
        opencv_imgproc
        opencv_core)
    
    target_compile_definitions(${TARGET_NAME} PUBLIC
        EXAMPLE_HAS_OPENCV)
endif()

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")