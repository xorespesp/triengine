########################################
##### Top-level CMake project file #####
########################################

cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

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

# EXPORT name used by `install(TARGETS ...)` and `install(EXPORT ...)` when exporting the library.
# (Both must be identical)
set(TRIENGINE_TARGETS_EXPORT_NAME "${TRIENGINE_PROJECT_NAME}TargetsExport")

# Install path for the config files generated when exporting the library.
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

option(TRIENGINE_FORCE_DISCRETE_GPU "Force usage of discrete GPU (NVIDIA Optimus, AMD PowerXpress)" ON)
option(TRIENGINE_ENABLE_MSVC_DYNAMIC_DEBUG "Enable dynamic-debugging compile option (MSVC only)" ON)
option(TRIENGINE_BUILD_EXAMPLES "Build examples" ON)
option(TRIENGINE_BUILD_EXTERNAL_EXAMPLES "Build external examples" OFF)

message(STATUS "--------------------------------------------------")
message(STATUS "TRIENGINE_LIBRARY_NAME: ${TRIENGINE_LIBRARY_NAME}")
message(STATUS "TRIENGINE_TARGETS_EXPORT_NAME: ${TRIENGINE_TARGETS_EXPORT_NAME}")
message(STATUS "TRIENGINE_CONFIG_INSTALL_DIR: ${TRIENGINE_CONFIG_INSTALL_DIR}")
message(STATUS "TRIENGINE_PROJECT_ROOT_DIR: ${TRIENGINE_PROJECT_ROOT_DIR}")
message(STATUS "TRIENGINE_FORCE_DISCRETE_GPU: ${TRIENGINE_FORCE_DISCRETE_GPU}")
message(STATUS "TRIENGINE_ENABLE_MSVC_DYNAMIC_DEBUG: ${TRIENGINE_ENABLE_MSVC_DYNAMIC_DEBUG}")
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
    
    if (TRIENGINE_ENABLE_MSVC_DYNAMIC_DEBUG)
        # Ref: https://devblogs.microsoft.com/cppblog/cpp-dynamic-debugging-full-debuggability-for-optimized-builds/#other-build-systems
        #message(STATUS "Enable MSVC dynamic-debugging globally...")
    
        # Pass /dynamicdeopt switch to cl.exe (requires /Zi or /Z7)
        add_compile_options(
            "$<$<NOT:$<CONFIG:Debug>>:/dynamicdeopt>"
            "$<$<NOT:$<CONFIG:Debug>>:/Zi>"
        )

        # Pass /dynamicdeopt switch to link.exe (requires /DEBUG:FULL)
        add_link_options(
            "$<$<NOT:$<CONFIG:Debug>>:/dynamicdeopt>"
            "$<$<NOT:$<CONFIG:Debug>>:/DEBUG:FULL>"
        )
        
        # Pass /dynamicdeopt switch to lib.exe (release only)
        string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELEASE " /dynamicdeopt") # Release
        string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO " /dynamicdeopt") # RelWithDebInfo

    endif()
endif()

########################################################################################

add_subdirectory(src)

if (TRIENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

########################################################################################

if (TRIENGINE_BUILD_EXTERNAL_EXAMPLES)
    # --- Build the example-external target after installing the library (to validate the Triengine install package) ---

    # Use configure_file to generate the actual build script from the template.
    # The generated file is placed in the build directory(CMAKE_CURRENT_BINARY_DIR).
    set(EXTERNAL_TARGET_PATH "${TRIENGINE_PROJECT_ROOT_DIR}/example-external") # subdirectory name that holds the example sources
    set(EXTERNAL_TARGET_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
    set(EXTERNAL_TARGET_CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    configure_file(
        "${TRIENGINE_PROJECT_ROOT_DIR}/cmake/BuildExternalTarget.cmake.in" # input
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake" # output
        @ONLY # substitute only `@VAR@`-style variables at configure time, leaving `${VAR}`-style variables intact (evaluated when the script runs)
    )

    # Specify the script to run after installation completes.
    # CMAKE_INSTALL_PREFIX is the install path the user specifies via cmake --install . --prefix <path>
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cmake/BuildExternalTarget.cmake")
endif() # ^^^ TRIENGINE_BUILD_EXTERNAL_EXAMPLES ^^^

########################################################################################