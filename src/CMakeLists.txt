set(TRIENGINE_LIBRARY_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}")
set(TRIENGINE_LIBRARY_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}")

########################################################################################
# library sources
set(TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/camera.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/image_format.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/image_buffer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/scene.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/text_object.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/global_options.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/gl_context.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/texture.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/frame_buffer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/shader.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/shader_preprocessor.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/shader_loader.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/geometry_gpu_resources.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/gpu_resource_manager.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/scene_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/core/bloom_effect.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/geometry_object_base.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/lineset_object.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/pcd_object.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/skeleton_object.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/mesh_object.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/geometry/mesh_object_factory.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/infinite_plane_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/light_source_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/lineset_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/pcd_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/skeleton_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/mesh_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/renderer/text_renderer.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/gui/gui_manager.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/gui/windows/log_window.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/gui/windows/scene_ctrl_window.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/gui/windows/scene_view_window.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/io/file_bvh_loader.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/io/file_obj_loader.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/io/file_ply_loader.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/visualization/visualizer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/visualization/visualizer_gui.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/visualization/offscreen_renderer.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/visualization/offscreen_renderer_dx.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/utility/logger.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/utility/debug_utils.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/utility/gl_utils.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/utility/color_map.cc"

    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/FontAwesome5.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/FontAwesome5Brands.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoBold.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoItalic.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoBold.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoItalic.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoRegular.cc"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/fonts/RobotoRegular.cc"
)

# library headers (public)
set(TRIENGINE_LIBRARY_HEADERS_PUBLIC
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/common.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/basic_types.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/camera.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/image_format.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/image_buffer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/texture_params.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/scene.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/lighting_options.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/infinite_plane_options.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/text_object.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/text_render_options.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/global_options.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/gl_context.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/texture.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/frame_buffer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/shader.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/shader_preprocessor.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/shader_loader.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/render_target.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/geometry_gpu_resources.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/geometry_gpu_resource_pool_alloc.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/gpu_resource_manager.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/scene_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/core/bloom_effect.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/geometry/geometry_object_base.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/geometry/lineset_object.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/geometry/pcd_object.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/geometry/skeleton_object.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/geometry/mesh_object.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/infinite_plane_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/light_source_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/lineset_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/pcd_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/renderer_base.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/skeleton_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/mesh_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/renderer/text_renderer.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/gui_manager.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/iwindow.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/widgets/file_browser_widget.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/windows/log_window.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/windows/render_stats_window.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/windows/scene_ctrl_window.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/gui/windows/scene_view_window.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/io/file_bvh_loader.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/io/file_obj_loader.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/io/file_ply_loader.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/io/pointcloud_load_options.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/math/constants.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/math/glm_utils.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/math/math3d.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/bit_cast.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/spin_lock.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/logger.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/debug_utils.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/gl_utils.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/color_map.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/hash_utils.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/noncopyable.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/singleton.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/scrolling_buffer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/progress_reporter.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/string_format.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/utility/string_tokenizer.hh"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/visualization/offscreen_renderer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/visualization/offscreen_renderer_dx.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/visualization/visualizer.hh"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/visualization/visualizer_gui.hh"
)

# library headers (private)
set(TRIENGINE_LIBRARY_HEADERS_PRIVATE
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/fonts/Fonts.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/fonts/IconsFontAwesome5.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/fonts/IconsFontAwesome5Brands.h"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/smaa_area_texture.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/smaa_search_texture.h"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/stb_image.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/tiny_obj_loader.h"

    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/wglext.h"
)
########################################################################################

########################################################################################
# glad sources
list(APPEND TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/glad/gl.c"
)

# glad headers
list(APPEND TRIENGINE_LIBRARY_HEADERS_PUBLIC
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/glad/gl.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/glad/KHR/khrplatform.h"
)
########################################################################################

########################################################################################
# imgui sources
list(APPEND TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_demo.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_draw.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_tables.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_widgets.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_impl_glfw.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/imgui/imgui_impl_opengl3.cpp"
)

# imgui headers
list(APPEND TRIENGINE_LIBRARY_HEADERS_PUBLIC
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imconfig.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imgui.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imgui_internal.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imstb_rectpack.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imstb_textedit.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imstb_truetype.h"    
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_glfw.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_opengl3_loader.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_opengl3.h"
)
########################################################################################

########################################################################################
# implot sources
list(APPEND TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/implot/implot.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/implot/implot_demo.cpp"
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/implot/implot_items.cpp"
)

# implot headers
list(APPEND TRIENGINE_LIBRARY_HEADERS_PUBLIC
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/implot/implot.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/implot/implot_internal.h"
)
########################################################################################

########################################################################################
# RPly sources
list(APPEND TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/rply/rply.c"
)

# RPly headers
list(APPEND TRIENGINE_LIBRARY_HEADERS_PRIVATE
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/rply/rply.h"
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/rply/rplyfile.h"
)
########################################################################################

########################################################################################
# miniz sources
list(APPEND TRIENGINE_LIBRARY_SOURCES
    "${TRIENGINE_LIBRARY_SOURCES_DIR}/extern/miniz/miniz.c"
)

# miniz headers
list(APPEND TRIENGINE_LIBRARY_HEADERS_PRIVATE
    "${TRIENGINE_LIBRARY_HEADERS_DIR}/extern/miniz/miniz.h"
)
########################################################################################

# Library Target Definition
add_library(${TRIENGINE_LIBRARY_NAME} STATIC)

# Library Target Alias Definition
#   - <TRIENGINE_PROJECT_NAME>::<TRIENGINE_LIBRARY_NAME> is alias of: <TRIENGINE_LIBRARY_NAME>
add_library(${TRIENGINE_PROJECT_NAME}::${TRIENGINE_LIBRARY_NAME} ALIAS ${TRIENGINE_LIBRARY_NAME})

# Add sources and headers to the library target
target_sources(${TRIENGINE_LIBRARY_NAME}
    PRIVATE
        ${TRIENGINE_LIBRARY_SOURCES}
        ${TRIENGINE_LIBRARY_HEADERS_PRIVATE}
    PUBLIC
        FILE_SET triengine_public_headers
            TYPE HEADERS
            FILES ${TRIENGINE_LIBRARY_HEADERS_PUBLIC}
            BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(${TRIENGINE_LIBRARY_NAME} PUBLIC cxx_std_17)

# Add compile definitions for targets
if (TRIENGINE_ENABLE_DEBUG_MODE)
    target_compile_definitions(${TRIENGINE_LIBRARY_NAME} PUBLIC
        "${TRIENGINE_PROJECT_NAME_UPPERCASE}_DEBUG_MODE=1")
endif()

if (TRIENGINE_ACTIVE_LOG_LEVEL)
    target_compile_definitions(${TRIENGINE_LIBRARY_NAME} PUBLIC
        "${TRIENGINE_PROJECT_NAME_UPPERCASE}_ACTIVE_LOG_LEVEL=${TRIENGINE_PROJECT_NAME_UPPERCASE}_LOG_LEVEL_${TRIENGINE_ACTIVE_LOG_LEVEL}")
endif()

target_compile_definitions(${TRIENGINE_LIBRARY_NAME}
    PUBLIC
        NOMINMAX
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    PRIVATE
        STB_IMAGE_IMPLEMENTATION
        TINYOBJLOADER_IMPLEMENTATION
)

if (TRIENGINE_FORCE_DISCRETE_GPU)
    target_compile_definitions(${TRIENGINE_LIBRARY_NAME} PUBLIC
        TRIENGINE_FORCE_DISCRETE_GPU
    )
endif()

if (MSVC)
    message(STATUS "Adding _CRT_SECURE_NO_WARNINGS compile definition to ${TRIENGINE_LIBRARY_NAME} ...")
    target_compile_definitions(${TRIENGINE_LIBRARY_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS)
endif()

# Global includes. Used by all targets
# Use generator expressions to specify different paths for the build tree
# and the install tree. This is the standard way to handle installable targets.
target_include_directories(${TRIENGINE_LIBRARY_NAME}
    PUBLIC
        # $<BUILD_INTERFACE:...>: Path used by consumers within the same build tree.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}/extern>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}/extern/glad> # for `#include <KHR/khrplatform.h>`
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}/extern/imgui> # for `#include "imgui.h"` (implot use this)
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TRIENGINE_LIBRARY_NAME}/extern/rply> # for `#include "rply.h"`, `#include "rplyfile.h`
        # $<INSTALL_INTERFACE:...>: Path relative to CMAKE_INSTALL_INCLUDEDIR used by consumers after installation
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TRIENGINE_LIBRARY_NAME}/extern>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TRIENGINE_LIBRARY_NAME}/extern/glad> # for `#include <KHR/khrplatform.h>`
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TRIENGINE_LIBRARY_NAME}/extern/imgui> # for `#include "imgui.h"` (implot use this)
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TRIENGINE_LIBRARY_NAME}/extern/rply> # for `#include "rply.h"`, `#include "rplyfile.h`
    PRIVATE
        # Path needed for compiling this library's own sources.
        ${CMAKE_CURRENT_SOURCE_DIR}
        # Build-tree root that holds the generated packed-shaders header
        # (making it reachable as `#include <triengine_generated/packed_shaders_data.h>`)
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)


########################################################################################
# Dependencies

find_package(OpenGL REQUIRED)

include(FetchEigen)
include(FetchGlfw)
include(FetchFreeType)

target_link_libraries(${TRIENGINE_LIBRARY_NAME}
    PUBLIC
        OpenGL::GL
        glfw
        Eigen3::Eigen
    PRIVATE
        freetype
)

########################################################################################

################################################################################
# Shaders Packing & Embedding
message(STATUS "Configuring shader packing and embedding...")

set(TRIENGINE_SHADERS_BASE_DIR "${TRIENGINE_LIBRARY_SOURCES_DIR}/shaders")
set(TRIENGINE_AUTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TRIENGINE_LIBRARY_NAME}_generated")
set(TRIENGINE_PACKED_SHADERS_HEADER_FILE "${TRIENGINE_AUTO_GENERATED_DIR}/packed_shaders_data.h")
set(TRIENGINE_SHADERS_BINPACK_SCRIPT "${TRIENGINE_PROJECT_ROOT_DIR}/scripts/binpack.py")

message(STATUS "TRIENGINE_SHADERS_BASE_DIR: ${TRIENGINE_SHADERS_BASE_DIR}")
message(STATUS "TRIENGINE_PACKED_SHADERS_HEADER_FILE: ${TRIENGINE_PACKED_SHADERS_HEADER_FILE}")
message(STATUS "TRIENGINE_SHADERS_BINPACK_SCRIPT: ${TRIENGINE_SHADERS_BINPACK_SCRIPT}")

# Ensure the parent directory of the generated header exists
get_filename_component(TRIENGINE_PACKED_SHADERS_HEADER_DIR ${TRIENGINE_PACKED_SHADERS_HEADER_FILE} DIRECTORY)
file(MAKE_DIRECTORY ${TRIENGINE_PACKED_SHADERS_HEADER_DIR})

# GLOB the absolute-path file list of assets (shaders) to embed into the binary
# NOTE: GLOB is evaluated at CMake configure time, NOT at build time, so newly added files are not detected automatically.
#       Therefore, after adding a new file, re-run CMake manually to refresh the SHADER_FILES variable.
file(GLOB_RECURSE TRIENGINE_SHADER_FILES
    "${TRIENGINE_SHADERS_BASE_DIR}/*.vert"
    "${TRIENGINE_SHADERS_BASE_DIR}/*.geom"
    "${TRIENGINE_SHADERS_BASE_DIR}/*.frag"
    "${TRIENGINE_SHADERS_BASE_DIR}/*.comp"
    "${TRIENGINE_SHADERS_BASE_DIR}/*.glsl"
    "${TRIENGINE_SHADERS_BASE_DIR}/*.hlsl"
)

if(NOT TRIENGINE_SHADER_FILES)
    message(WARNING "No shader files were found. Check the patterns in file(GLOB_RECURSE).")
else()
    message(STATUS "Shader files found: ${TRIENGINE_SHADER_FILES}")
endif()

# NOTE: A Python3 interpreter is required to run the embed script
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Add a custom command that packs & embeds the asset files (shaders)
add_custom_command(
    # OUTPUT: the product of this command
    OUTPUT ${TRIENGINE_PACKED_SHADERS_HEADER_FILE}
    # COMMAND: the command to run. (python3 <script> <output header> <variable name> <base path> <file list...>)
    COMMAND ${Python3_EXECUTABLE} ${TRIENGINE_SHADERS_BINPACK_SCRIPT} 
            ${TRIENGINE_PACKED_SHADERS_HEADER_FILE} 
            "kTrienginePackedShaderData" 
            ${TRIENGINE_SHADERS_BASE_DIR} 
            ${TRIENGINE_SHADER_FILES} # <- (CMake automatically expands the list into individual arguments)
    # DEPENDS: the list of files this command depends on. (the command runs if any of TRIENGINE_SHADER_FILES changes)
    #          Also add the packing script itself as a dependency, so regeneration is triggered when the script is modified too
    DEPENDS ${TRIENGINE_SHADER_FILES} ${TRIENGINE_SHADERS_BINPACK_SCRIPT}
    VERBATIM # Added to correctly handle special characters on the command line
    COMMENT "Packing and embedding shaders into ${TRIENGINE_PACKED_SHADERS_HEADER_FILE}..."
)

# Create a custom target (AssetPacker) that wraps the command above
# This target depends on TRIENGINE_PACKED_SHADERS_HEADER_FILE, the product of the command above.
# Adding the 'ALL' keyword includes it in the default build without having to specify the target explicitly.
add_custom_target(TriengineShadersPacker ALL
    DEPENDS ${TRIENGINE_PACKED_SHADERS_HEADER_FILE}
)

# Make the main target depend on the custom target (AssetPacker) (this is the key part)
# Ensure the AssetPacker target is up to date before building the main target.
add_dependencies(${TRIENGINE_LIBRARY_NAME}
    TriengineShadersPacker
)
################################################################################

# Install Library Targets
install(
    TARGETS "${TRIENGINE_LIBRARY_NAME}"
    EXPORT "${TRIENGINE_TARGETS_EXPORT_NAME}" # Associates this target with the export set named below
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs static libraries (.a, .lib)
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs shared libraries (.so, .dll) - safe to include even for STATIC only
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" # Installs executables or Windows DLLs
    # INCLUDES: Specifies the base directory for <INSTALL_INTERFACE:...> used above.
    # Tells consuming projects that include paths are relative to <prefix>/${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    # Install the public headers registered in a `FILE_SET` via `target_sources()`.
    # (The directory structure relative to `BASE_DIRS` is preserved under the installation destination.)
    FILE_SET triengine_public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Install Library Resources
#   - resources/*.* -> <prefix>/resources
install(
    DIRECTORY "${TRIENGINE_PROJECT_ROOT_DIR}/resources/" # The trailing '/' copies the *contents* of "resources"
    DESTINATION "resources" # Target: "<prefix>/resources"
)

# Install Library Config
# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
install(
    EXPORT "${TRIENGINE_TARGETS_EXPORT_NAME}"
    FILE "${TRIENGINE_PROJECT_NAME}Targets.cmake"
    DESTINATION "${TRIENGINE_CONFIG_INSTALL_DIR}"
    NAMESPACE "${TRIENGINE_PROJECT_NAME}::"
)

# Configure '<TRIENGINE_PROJECT_NAME>Config.cmake'
# Use variables:
#   - TARGETS_EXPORT_NAME
#   - TRIENGINE_PROJECT_NAME
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/LibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${TRIENGINE_PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${TRIENGINE_CONFIG_INSTALL_DIR}"
)

# Copy generated config file '<TRIENGINE_PROJECT_NAME>Config.cmake' to install directory
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${TRIENGINE_PROJECT_NAME}Config.cmake"
    DESTINATION "${TRIENGINE_CONFIG_INSTALL_DIR}"
)