# example-external
cmake_minimum_required(VERSION 3.20)
project(example-external)

#set(CMAKE_PREFIX_PATH "/path/to/triengine/install")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

add_executable(example-external)
target_sources(example-external
    PRIVATE
        "src/example-external.cc"
)

find_package(Triengine CONFIG REQUIRED)

target_link_libraries(example-external PRIVATE 
    Triengine::triengine)
