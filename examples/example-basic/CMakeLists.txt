set(TARGET_NAME example-basic)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/basic_demo_app.hh"
        "src/basic_demo_app.cc"
        "src/example_basic_main.cc"
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