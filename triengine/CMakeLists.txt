set(LIBRARY_SOURCES_DIR "src/${LIBRARY_NAME}")
set(LIBRARY_HEADERS_DIR "src/${LIBRARY_NAME}")

############################################################
# library sources
set(LIBRARY_SOURCES
    "${LIBRARY_SOURCES_DIR}/camera.cc"
    "${LIBRARY_SOURCES_DIR}/texture.cc"
    "${LIBRARY_SOURCES_DIR}/scene.cc"
    "${LIBRARY_SOURCES_DIR}/global_options.cc"

    "${LIBRARY_SOURCES_DIR}/core/gl_context.cc"
    "${LIBRARY_SOURCES_DIR}/core/frame_buffer.cc"
    "${LIBRARY_SOURCES_DIR}/core/shader.cc"
    "${LIBRARY_SOURCES_DIR}/core/shader_preprocessor.cc"
    "${LIBRARY_SOURCES_DIR}/core/gpu_resource_manager.cc"
    "${LIBRARY_SOURCES_DIR}/core/gpu_resources_impl.cc"
    "${LIBRARY_SOURCES_DIR}/core/scene_renderer.cc"

    "${LIBRARY_HEADERS_DIR}/geometry/geometry_object_base.cc"
    "${LIBRARY_SOURCES_DIR}/geometry/lineset_object.cc"
    "${LIBRARY_SOURCES_DIR}/geometry/pcd_object.cc"
    "${LIBRARY_SOURCES_DIR}/geometry/skeleton_object.cc"
    "${LIBRARY_SOURCES_DIR}/geometry/triangle_mesh_object.cc"
    "${LIBRARY_SOURCES_DIR}/geometry/triangle_mesh_object_factory.cc"

    "${LIBRARY_SOURCES_DIR}/renderer/infinite_grid_renderer.cc"
    "${LIBRARY_SOURCES_DIR}/renderer/light_source_renderer.cc"
    "${LIBRARY_SOURCES_DIR}/renderer/lineset_renderer.cc"
    "${LIBRARY_SOURCES_DIR}/renderer/pcd_renderer.cc"
    "${LIBRARY_SOURCES_DIR}/renderer/skeleton_renderer.cc"
    "${LIBRARY_SOURCES_DIR}/renderer/triangle_mesh_renderer.cc"

    "${LIBRARY_SOURCES_DIR}/gui/gui_manager.cc"
    "${LIBRARY_SOURCES_DIR}/gui/windows/scene_ctrl_window.cc"
    "${LIBRARY_SOURCES_DIR}/gui/windows/scene_view_window.cc"

    "${LIBRARY_SOURCES_DIR}/io/file_bvh_loader.cc"
    "${LIBRARY_SOURCES_DIR}/io/file_obj_loader.cc"
    "${LIBRARY_SOURCES_DIR}/io/file_ply_loader.cc"

    "${LIBRARY_SOURCES_DIR}/visualization/visualizer.cc"
    "${LIBRARY_SOURCES_DIR}/visualization/offscreen_renderer.cc"

    "${LIBRARY_SOURCES_DIR}/utility/debug_utils.cc"
    "${LIBRARY_SOURCES_DIR}/utility/gl_utils.cc"

    "${LIBRARY_SOURCES_DIR}/extern/fonts/FontAwesome5.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/FontAwesome5Brands.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoBold.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoItalic.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoBold.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoItalic.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoMonoRegular.cc"
    "${LIBRARY_SOURCES_DIR}/extern/fonts/RobotoRegular.cc"
)

# library headers (public)
set(LIBRARY_HEADERS_PUBLIC
    "${LIBRARY_HEADERS_DIR}/common.h"
    "${LIBRARY_HEADERS_DIR}/basic_types.hh"
    "${LIBRARY_HEADERS_DIR}/camera.hh"
    "${LIBRARY_HEADERS_DIR}/image.hh"
    "${LIBRARY_HEADERS_DIR}/texture.hh"
    "${LIBRARY_HEADERS_DIR}/scene.hh"
    "${LIBRARY_HEADERS_DIR}/lighting_options.hh"
    "${LIBRARY_HEADERS_DIR}/infinite_grid_options.hh"
    "${LIBRARY_HEADERS_DIR}/global_options.hh"

    "${LIBRARY_HEADERS_DIR}/core/gl_context.hh"
    "${LIBRARY_HEADERS_DIR}/core/frame_buffer.hh"
    "${LIBRARY_HEADERS_DIR}/core/shader.hh"
    "${LIBRARY_HEADERS_DIR}/core/shader_preprocessor.hh"
    "${LIBRARY_HEADERS_DIR}/core/render_target.hh"
    "${LIBRARY_HEADERS_DIR}/core/gpu_resource_manager.hh"
    "${LIBRARY_HEADERS_DIR}/core/scene_renderer.hh"

    "${LIBRARY_HEADERS_DIR}/geometry/geometry_object_base.hh"
    "${LIBRARY_HEADERS_DIR}/geometry/lineset_object.hh"
    "${LIBRARY_HEADERS_DIR}/geometry/pcd_object.hh"
    "${LIBRARY_HEADERS_DIR}/geometry/skeleton_object.hh"
    "${LIBRARY_HEADERS_DIR}/geometry/triangle_mesh_object.hh"

    "${LIBRARY_HEADERS_DIR}/renderer/infinite_grid_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/light_source_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/lineset_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/pcd_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/renderer_base.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/skeleton_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/renderer/triangle_mesh_renderer.hh"

    "${LIBRARY_HEADERS_DIR}/gui/gui_manager.hh"
    "${LIBRARY_HEADERS_DIR}/gui/iwindow.hh"
    "${LIBRARY_HEADERS_DIR}/gui/windows/log_window.hh"
    "${LIBRARY_HEADERS_DIR}/gui/windows/render_stats_window.hh"
    "${LIBRARY_HEADERS_DIR}/gui/windows/scene_ctrl_window.hh"
    "${LIBRARY_HEADERS_DIR}/gui/windows/scene_view_window.hh"

    "${LIBRARY_HEADERS_DIR}/io/file_bvh_loader.hh"
    "${LIBRARY_HEADERS_DIR}/io/file_obj_loader.hh"
    "${LIBRARY_HEADERS_DIR}/io/file_ply_loader.hh"
    "${LIBRARY_HEADERS_DIR}/io/pointcloud_load_options.hh"

    "${LIBRARY_HEADERS_DIR}/math/constants.hh"
    "${LIBRARY_HEADERS_DIR}/math/glm_utils.hh"
    "${LIBRARY_HEADERS_DIR}/math/math3d.hh"

    "${LIBRARY_HEADERS_DIR}/utility/bit_cast.hh"
    "${LIBRARY_HEADERS_DIR}/utility/spin_lock.hh"
    "${LIBRARY_HEADERS_DIR}/utility/debug_utils.hh"
    "${LIBRARY_HEADERS_DIR}/utility/gl_utils.hh"
    "${LIBRARY_HEADERS_DIR}/utility/hash_utils.hh"
    "${LIBRARY_HEADERS_DIR}/utility/noncopyable.hh"
    "${LIBRARY_HEADERS_DIR}/utility/singleton.hh"
    "${LIBRARY_HEADERS_DIR}/utility/progress_reporter.hh"
    "${LIBRARY_HEADERS_DIR}/utility/string_format.hh"
    "${LIBRARY_HEADERS_DIR}/utility/string_tokenizer.hh"

    "${LIBRARY_HEADERS_DIR}/shaders/infinite_grid_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/light_source_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/lineset_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/normal_vis_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/overlay_pass_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/pcd_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/screen_quad_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/shader_version.h"
    "${LIBRARY_HEADERS_DIR}/shaders/smaa_pass_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/triangle_mesh_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/wboit_composite_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/includes/phong_lighting_shaders.h"
    "${LIBRARY_HEADERS_DIR}/shaders/includes/smaa_shaders.h"

    "${LIBRARY_HEADERS_DIR}/visualization/offscreen_renderer.hh"
    "${LIBRARY_HEADERS_DIR}/visualization/visualizer.hh"
)

# library headers (private)
set(LIBRARY_HEADERS_PRIVATE
    "${LIBRARY_HEADERS_DIR}/extern/fonts/Fonts.h"
    "${LIBRARY_HEADERS_DIR}/extern/fonts/IconsFontAwesome5.h"
    "${LIBRARY_HEADERS_DIR}/extern/fonts/IconsFontAwesome5Brands.h"

    "${LIBRARY_HEADERS_DIR}/extern/smaa_area_texture.h"
    "${LIBRARY_HEADERS_DIR}/extern/smaa_search_texture.h"

    "${LIBRARY_HEADERS_DIR}/extern/stb_image.h"
    "${LIBRARY_HEADERS_DIR}/extern/tiny_obj_loader.h"
)
############################################################

############################################################
# glad sources
list(APPEND LIBRARY_SOURCES
    "${LIBRARY_SOURCES_DIR}/extern/glad/glad.c"
)

# glad headers
list(APPEND LIBRARY_HEADERS_PUBLIC
    "${LIBRARY_HEADERS_DIR}/extern/glad/glad.h"
    "${LIBRARY_HEADERS_DIR}/extern/glad/KHR/khrplatform.h"
)
############################################################

############################################################
# imgui sources
list(APPEND LIBRARY_SOURCES
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_demo.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_draw.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_tables.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_widgets.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_impl_glfw.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/imgui/imgui_impl_opengl3.cpp"
)

# imgui headers
list(APPEND LIBRARY_HEADERS_PUBLIC
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imconfig.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imgui.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imgui_internal.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imstb_rectpack.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imstb_textedit.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imstb_truetype.h"    
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_glfw.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_opengl3_loader.h"
    "${LIBRARY_HEADERS_DIR}/extern/imgui/imgui_impl_opengl3.h"
)
############################################################

############################################################
# implot sources
list(APPEND LIBRARY_SOURCES
    "${LIBRARY_SOURCES_DIR}/extern/implot/implot.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/implot/implot_demo.cpp"
    "${LIBRARY_SOURCES_DIR}/extern/implot/implot_items.cpp"
)

# implot headers
list(APPEND LIBRARY_HEADERS_PUBLIC
    "${LIBRARY_HEADERS_DIR}/extern/implot/implot.h"
    "${LIBRARY_HEADERS_DIR}/extern/implot/implot_internal.h"
)
############################################################

############################################################
# RPly sources
list(APPEND LIBRARY_SOURCES
    "${LIBRARY_SOURCES_DIR}/extern/rply/rply.c"
)

# RPly headers
list(APPEND LIBRARY_HEADERS_PUBLIC
    "${LIBRARY_SOURCES_DIR}/extern/rply/rply.h"
    "${LIBRARY_SOURCES_DIR}/extern/rply/rplyfile.h"
)
############################################################

# Library Target Definition
add_library(${LIBRARY_NAME} STATIC
    ${LIBRARY_SOURCES}
    ${LIBRARY_HEADERS_PUBLIC}
    ${LIBRARY_HEADERS_PRIVATE}
)

# Library Target Alias Definition
#   - <PROJECT_NAME>::<LIBRARY_NAME> is alias of: <LIBRARY_NAME>
add_library(${PROJECT_NAME}::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_17)

# Add compile definitions for targets
target_compile_definitions(${LIBRARY_NAME} PUBLIC
  "${PROJECT_NAME_UPPERCASE}_DEBUG_MODE=$<CONFIG:Debug>")

target_compile_definitions(${LIBRARY_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    STB_IMAGE_IMPLEMENTATION
    TINYOBJLOADER_IMPLEMENTATION
    NOMINMAX
)

# Global includes. Used by all targets
# Use generator expressions to specify different paths for the build tree
# and the install tree. This is the standard way to handle installable targets.
target_include_directories(${LIBRARY_NAME}
    PUBLIC
        # $<BUILD_INTERFACE:...>: Path used by consumers within the same build tree.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${LIBRARY_NAME}/extern>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${LIBRARY_NAME}/extern/glad> # for `#include <KHR/khrplatform.h>`
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${LIBRARY_NAME}/extern/imgui> # for `#include "imgui.h"` (implot use this)
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${LIBRARY_NAME}/extern/rply> # for `#include "rply.h"`, `#include "rplyfile.h`
        # $<INSTALL_INTERFACE:...>: Path relative to CMAKE_INSTALL_INCLUDEDIR used by consumers after installation
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME}/extern>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME}/extern/glad> # for `#include <KHR/khrplatform.h>`
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME}/extern/imgui> # for `#include "imgui.h"` (implot use this)
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME}/extern/rply> # for `#include "rply.h"`, `#include "rplyfile.h`
    PRIVATE
        # Path needed for compiling this library's own sources.
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)


##################################################
# Dependencies

find_package(OpenGL REQUIRED)

include(FetchEigen)
include(FetchGlfw)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        OpenGL::GL
        glfw
        Eigen3::Eigen
)

##################################################

# Install Targets:
install(
    TARGETS "${LIBRARY_NAME}"
    EXPORT "${TARGETS_EXPORT_NAME}" # Associates this target with the export set named below
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs static libraries (.a, .lib)
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" # Installs shared libraries (.so, .dll) - safe to include even for STATIC only
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" # Installs executables or Windows DLLs
    # INCLUDES: Specifies the base directory for <INSTALL_INTERFACE:...> used above.
    # Tells consuming projects that include paths are relative to <prefix>/${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Install public headers
#   - src/<LIBRARY_NAME>/*.h,*.hh,*.hpp -> <prefix>/include/<LIBRARY_NAME>/*.h,*.hh,*.hpp
set(INSTALL_HEADER_EXCLUDE_PATTERNS "")
foreach(PRIVATE_HEADER ${LIBRARY_HEADERS_PRIVATE})
    list(APPEND INSTALL_HEADER_EXCLUDE_PATTERNS PATTERN ${PRIVATE_HEADER} EXCLUDE)
endforeach()
message(STATUS "INSTALL_HEADER_EXCLUDE_PATTERNS : ${INSTALL_HEADER_EXCLUDE_PATTERNS}")
install(
    DIRECTORY "src/${LIBRARY_NAME}/" # The trailing '/' copies the *contents* of "src/<LIBRARY_NAME>"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME}" # Target: <prefix>/include/<LIBRARY_NAME>
    FILES_MATCHING # Only install matched files
    PATTERN "*.hpp"
    PATTERN "*.hh"
    PATTERN "*.h"
    ${INSTALL_HEADER_EXCLUDE_PATTERNS} # Exclude private headers
)

# Install Resources
#   - resources/*.* -> <prefix>/resources
install(
    DIRECTORY "resources/" # The trailing '/' copies the *contents* of "resources"
    DESTINATION "resources" # Target: "<prefix>/resources"
)

# Install Config
# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
install(
    EXPORT "${TARGETS_EXPORT_NAME}"
    FILE "${PROJECT_NAME}Targets.cmake"
    DESTINATION "${CONFIG_INSTALL_DIR}"
    NAMESPACE "${PROJECT_NAME}::"
)

# Configure '<PROJECT-NAME>Config.cmake'
# Use variables:
#   - TARGETS_EXPORT_NAME
#   - PROJECT_NAME
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/LibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CONFIG_INSTALL_DIR}"
)

# 생성된 mylibconfig.cmake 파일을 install 경로에 복사
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    DESTINATION "${CONFIG_INSTALL_DIR}"
)