set(TARGET_NAME example-basic)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/example_basic_main.cc"
        "src/basic_demo_app.cc"
        "src/basic_demo_app.hh"
        "src/scene_wrapper.hh"
        "src/scene/main_scene.hh"
        "src/scene/engine_scene.hh"
        "src/scene/pcd_scene.hh"
        "src/scene/bvh_scene.hh"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

target_link_libraries(${TARGET_NAME} PRIVATE 
    Triengine::triengine
    example-common)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")