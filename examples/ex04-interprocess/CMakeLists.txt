set(TARGET_NAME ex04-interprocess)
set(ENABLE_OPENCV OFF)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/task_dispatcher.hh"
        "src/scene_wrapper.hh"
        "src/scene_manager.hh"
        "src/scene/skull_scene.hh"
        "src/scene/plasma_scene.hh"
        "src/scene/plasma_scene.cc"
        "src/renderer_process.cc"
        "src/renderer_process.hh"
        "src/viewer_process.cc"
        "src/viewer_process.hh"
        "src/ex04_interprocess_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

include(FetchTriengineInterop)

target_link_libraries(${TARGET_NAME} PRIVATE
    Triengine::triengine
    Triengine::interop
    Triengine::interop_surface
    xutl)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")