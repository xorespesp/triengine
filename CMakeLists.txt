########################################
##### Top-level CMake project file #####
########################################

cmake_minimum_required(VERSION 3.20)

# 루트 프로젝트명
set(PROJECT_NAME Triengine)

project(${PROJECT_NAME}
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Triengine Library"
)

####################################################################

# Use standard install directory variables
#   - CMAKE_INSTALL_LIBDIR
#   - CMAKE_INSTALL_BINDIR
#   - CMAKE_INSTALL_INCLUDEDIR
include(GNUInstallDirs)

# Include module with functions:
# Generates and installs the files needed for find_package(${LIBRARY_NAME}) to work.
#   - write_basic_package_version_file(...)
#   - configure_package_config_file(...)
include(CMakePackageConfigHelpers)

include(FetchContent)

########################################################################################

# --- Default Install Prefix Handling (Optional but recommended) ---
# Sets a default installation path if the user doesn't specify one via -DCMAKE_INSTALL_PREFIX
message(STATUS "Current CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMAKE_INSTALL_PREFIX has been not set
    message(STATUS "Overriding CMAKE_INSTALL_PREFIX to: ${CMAKE_SOURCE_DIR}/install")
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_SOURCE_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
endif()
message(STATUS "Effective CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Install directories: LIB=${CMAKE_INSTALL_LIBDIR}, INCLUDE=${CMAKE_INSTALL_INCLUDEDIR}, BIN=${CMAKE_INSTALL_BINDIR}, CMAKE=${CMAKE_INSTALL_DATADIR}/cmake")

########################################################################################

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_EXTERNAL_EXAMPLES "Build external examples" OFF)

########################################################################################

# Set PROJECT_NAME_UPPERCASE and PROJECT_NAME_LOWERCASE variables
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPERCASE)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)

# Library name (by default is the project name)
if(NOT LIBRARY_NAME)
  set(LIBRARY_NAME ${PROJECT_NAME_LOWERCASE})
endif()

# 라이브러리 export 시 install(TARGETS ...)와 install(EXPORT ...)에서 사용할 EXPORT 이름. 
# (둘이 반드시 같아야 함)
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}TargetsExport")

# 라이브러리 export 시 생성된 config 파일들이 install될 경로.
# This works for all platforms:
#   - <prefix>/lib*/cmake/<PROJECT-NAME>
#   - <prefix>/lib*/
#   - <prefix>/include/
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# where to find our CMake modules
# https://cmake.org/cmake/help/latest/variable/CMAKE_MODULE_PATH.html
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Make sure different configurations don't collide
set(CMAKE_DEBUG_POSTFIX "d")

# Standard C++ Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    message(STATUS "Suppressing MSVC warning C4819 globally...")
    add_compile_options(/wd4819) # or `target_compile_options(my_target PRIVATE /wd4819)`
endif()

########################################################################################

message(STATUS "--------------------------------------------------")
message(STATUS "BUILD_EXAMPLES: ${BUILD_EXAMPLES}")
message(STATUS "BUILD_EXTERNAL_EXAMPLES: ${BUILD_EXTERNAL_EXAMPLES}")
message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")
message(STATUS "LIBRARY_NAME: ${LIBRARY_NAME}")
message(STATUS "TARGETS_EXPORT_NAME: ${TARGETS_EXPORT_NAME}")
message(STATUS "CONFIG_INSTALL_DIR: ${CONFIG_INSTALL_DIR}")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "--------------------------------------------------")

########################################################################################

add_subdirectory(${LIBRARY_NAME})

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (BUILD_EXTERNAL_EXAMPLES)
    # --- mylib install 이후 example-external 타겟 빌드 수행 (mylib 설치 패키지의 유효성 검증 목적) ---

    # configure_file 을 사용하여 템플릿으로부터 실제 빌드 스크립트 생성
    # 생성된 파일은 빌드 디렉토리에 위치하게 됨 (CMAKE_CURRENT_BINARY_DIR)
    set(EXTERNAL_TARGET_PATH "${CMAKE_SOURCE_DIR}/example-external") # 예제 소스가 있는 하위 디렉토리 이름
    set(EXTERNAL_TARGET_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
    set(EXTERNAL_TARGET_CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/BuildExternalTarget.cmake.in" # input
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake" # output
        @ONLY # configure 시 `@VAR@` 형식의 변수만 치환하고 `${VAR}` 형식의 변수는 그대로 유지(스크립트 실행 시 평가).
    )

    # 설치가 완료된 후 실행될 스크립트 지정
    # CMAKE_INSTALL_PREFIX 는 사용자가 cmake --install . --prefix <경로> 로 지정한 설치 경로
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake")
endif() # ^^^ BUILD_EXTERNAL_EXAMPLES ^^^

########################################################################################