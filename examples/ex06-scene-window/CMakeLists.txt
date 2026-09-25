set(TARGET_NAME ex06-scene-window)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/ex06_scene_window_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

target_link_libraries(${TARGET_NAME} PRIVATE
    Triengine::triengine
    xutl)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")
