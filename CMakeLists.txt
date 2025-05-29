########################################
##### Top-level CMake project file #####
########################################

cmake_minimum_required(VERSION 3.20)

########################################################################################
# Library Root Includes / Policies

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
# Library Variables

# Root project name
set(TRIENGINE_PROJECT_NAME Triengine)

# Set TRIENGINE_PROJECT_NAME_UPPERCASE and TRIENGINE_PROJECT_NAME_LOWERCASE variables
string(TOUPPER ${TRIENGINE_PROJECT_NAME} TRIENGINE_PROJECT_NAME_UPPERCASE)
string(TOLOWER ${TRIENGINE_PROJECT_NAME} TRIENGINE_PROJECT_NAME_LOWERCASE)

# Library name (by default is the project name)
if(NOT TRIENGINE_LIBRARY_NAME)
  set(TRIENGINE_LIBRARY_NAME ${TRIENGINE_PROJECT_NAME_LOWERCASE})
endif()

# 라이브러리 export 시 install(TARGETS ...)와 install(EXPORT ...)에서 사용할 EXPORT 이름. 
# (둘이 반드시 같아야 함)
set(TRIENGINE_TARGETS_EXPORT_NAME "${TRIENGINE_PROJECT_NAME}TargetsExport")

# 라이브러리 export 시 생성된 config 파일들이 install될 경로.
# This works for all platforms:
#   - <prefix>/lib*/cmake/<PROJECT-NAME>
#   - <prefix>/lib*/
#   - <prefix>/include/
set(TRIENGINE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${TRIENGINE_PROJECT_NAME}")

# Use `TRIENGINE_PROJECT_ROOT_DIR` instead of `CMAKE_SOURCE_DIR`, due to avoid FetchContent issue.
# (see: https://stackoverflow.com/q/76539375)
set(TRIENGINE_PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

########################################################################################
# Library Configure Options

option(TRIENGINE_BUILD_EXAMPLES "Build examples" ON)
option(TRIENGINE_BUILD_EXTERNAL_EXAMPLES "Build external examples" OFF)

message(STATUS "--------------------------------------------------")
message(STATUS "TRIENGINE_LIBRARY_NAME: ${TRIENGINE_LIBRARY_NAME}")
message(STATUS "TRIENGINE_TARGETS_EXPORT_NAME: ${TRIENGINE_TARGETS_EXPORT_NAME}")
message(STATUS "TRIENGINE_CONFIG_INSTALL_DIR: ${TRIENGINE_CONFIG_INSTALL_DIR}")
message(STATUS "TRIENGINE_PROJECT_ROOT_DIR: ${TRIENGINE_PROJECT_ROOT_DIR}")
message(STATUS "TRIENGINE_BUILD_EXAMPLES: ${TRIENGINE_BUILD_EXAMPLES}")
message(STATUS "TRIENGINE_BUILD_EXTERNAL_EXAMPLES: ${TRIENGINE_BUILD_EXTERNAL_EXAMPLES}")
message(STATUS "--------------------------------------------------")

########################################################################################
# Library Root Project Setup

project(${TRIENGINE_PROJECT_NAME}
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Triengine Library"
)

# Standard C++ Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Make sure different configurations don't collide
set(CMAKE_DEBUG_POSTFIX "d")

# --- Default Install Prefix Handling (Optional but recommended) ---
# Sets a default installation path if the user doesn't specify one via -DCMAKE_INSTALL_PREFIX
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMAKE_INSTALL_PREFIX has been not set
    message(WARNING "Overriding CMAKE_INSTALL_PREFIX to: ${TRIENGINE_PROJECT_ROOT_DIR}/install")
    message(WARNING "(Original CMAKE_INSTALL_PREFIX was: ${CMAKE_INSTALL_PREFIX})")
    set(CMAKE_INSTALL_PREFIX
        "${TRIENGINE_PROJECT_ROOT_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
endif()
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Install directories: LIB=${CMAKE_INSTALL_LIBDIR}, INCLUDE=${CMAKE_INSTALL_INCLUDEDIR}, BIN=${CMAKE_INSTALL_BINDIR}, CMAKE=${CMAKE_INSTALL_DATADIR}/cmake")

# where to find our CMake modules
# https://cmake.org/cmake/help/latest/variable/CMAKE_MODULE_PATH.html
list(APPEND CMAKE_MODULE_PATH "${TRIENGINE_PROJECT_ROOT_DIR}/cmake")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

########################################################################################
# Library Root Compile Options

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_ID MATCHES "Clang")
    # Enable all warnings and treat them as errors
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (MSVC)
    add_compile_options(/W4)

    message(STATUS "Suppress MSVC warnings C4819 C4127 globally...")
    add_compile_options(/wd4819 /wd4127) # or `target_compile_options(my_target PRIVATE /wd4819 /wd4127)`

    message(STATUS "Enable MSVC /utf-8 switch globally...")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    
    # Ref: https://devblogs.microsoft.com/cppblog/cpp-dynamic-debugging-full-debuggability-for-optimized-builds/#other-build-systems
    message(STATUS "Enable MSVC dynamic-debugging globally...")
    
    # Pass /dynamicdeopt switch to cl.exe (requires /Zi or /Z7)
    add_compile_options(/dynamicdeopt /Zi)

    # Pass /dynamicdeopt switch to link.exe (requires /DEBUG:FULL)
    add_link_options(/dynamicdeopt /DEBUG:FULL)

    # Pass /dynamicdeopt switch to lib.exe
    string(APPEND CMAKE_STATIC_LINKER_FLAGS " /dynamicdeopt")
    message(STATUS "CMAKE_STATIC_LINKER_FLAGS: ${CMAKE_STATIC_LINKER_FLAGS}")
endif()

########################################################################################

add_subdirectory(${TRIENGINE_LIBRARY_NAME})

if (TRIENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

########################################################################################

if (TRIENGINE_BUILD_EXTERNAL_EXAMPLES)
    # --- 라이브러리 install 이후 example-external 타겟 빌드 수행 (Triengine 설치 패키지의 유효성 검증 목적) ---

    # configure_file 을 사용하여 템플릿으로부터 실제 빌드 스크립트 생성
    # 생성된 파일은 빌드 디렉토리에 위치하게 됨 (CMAKE_CURRENT_BINARY_DIR)
    set(EXTERNAL_TARGET_PATH "${TRIENGINE_PROJECT_ROOT_DIR}/example-external") # 예제 소스가 있는 하위 디렉토리명
    set(EXTERNAL_TARGET_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
    set(EXTERNAL_TARGET_CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    configure_file(
        "${TRIENGINE_PROJECT_ROOT_DIR}/cmake/BuildExternalTarget.cmake.in" # input
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake" # output
        @ONLY # configure 시 `@VAR@` 형식의 변수만 치환하고, `${VAR}` 형식의 변수는 그대로 유지(스크립트 실행 시 평가)하도록 하는 옵션
    )

    # 설치가 완료된 후 실행될 스크립트 지정
    # CMAKE_INSTALL_PREFIX 는 사용자가 cmake --install . --prefix <경로> 로 지정한 설치 경로
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake")
endif() # ^^^ TRIENGINE_BUILD_EXTERNAL_EXAMPLES ^^^

########################################################################################