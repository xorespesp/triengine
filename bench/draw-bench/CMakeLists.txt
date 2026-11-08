set(TARGET_NAME draw-bench)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/draw_bench_main.cc"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

include(FetchNlohmannJson)

target_link_libraries(${TARGET_NAME} PRIVATE
    Triengine::triengine
    nlohmann_json::nlohmann_json)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")
