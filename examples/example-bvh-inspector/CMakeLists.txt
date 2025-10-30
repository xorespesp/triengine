set(TARGET_NAME example-bvh-inspector)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/example_bvh_inspector_main.cc"
        "src/bvh_inspector_app.cc"
        "src/bvh_inspector_app.hh"
        "src/bvh_inspector_window.cc"
        "src/bvh_inspector_window.hh"
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