#pragma once
#include <triengine/common.h>
#include <triengine/core/shader_block_buffer.hh>

#include <cstdint>

namespace triengine::core
{
    class shader_program;
}

namespace triengine::renderer
{
    struct render_context;

    // `DirLight` of shaders/includes/phong_lighting.glsl
    struct glsl_dir_light
    {
        bool enabled;
        color3_f32 color;
        vec3_f32 directionInView;
        float ambientIntensity;
        float diffuseIntensity;
        float specularIntensity;
        TRIENGINE_GLSL_STRUCT(glsl_dir_light, enabled, color, directionInView, ambientIntensity, diffuseIntensity, specularIntensity)
    };

    // `PointLight` of shaders/includes/phong_lighting.glsl
    struct glsl_point_light
    {
        bool enabled;
        color3_f32 color;
        vec3_f32 positionInView;
        float Kc;
        float Kl;
        float Kq;
        float ambientIntensity;
        float diffuseIntensity;
        float specularIntensity;
        TRIENGINE_GLSL_STRUCT(glsl_point_light, enabled, color, positionInView, Kc, Kl, Kq, ambientIntensity, diffuseIntensity, specularIntensity)
    };

    // `SimpleFogOptions` of shaders/includes/simple_fog.glsl
    struct glsl_simple_fog
    {
        bool enabled;
        color3_f32 color;
        float density;
        float startDist;
        TRIENGINE_GLSL_STRUCT(glsl_simple_fog, enabled, color, density, startDist)
    };

    // `LightingBlock` of shaders/includes/lighting_block.glsl
    struct lighting_block
    {
        glsl_dir_light u_dirLight;
        glsl_point_light u_pointLight;
        glsl_simple_fog u_simpleFog;
        TRIENGINE_GLSL_BLOCK(lighting_block, "LightingBlock", u_dirLight, u_pointLight, u_simpleFog)
    };

    // `layout(std140) uniform` of `LightingBlock`
    using lighting_uniform_buffer = core::shader_block::uniform_buffer<lighting_block>;

    // `binding` of `LightingBlock`
    inline constexpr uint32_t kLightingBlockBindingPointIndex = 1;

    lighting_block make_lighting_block(const render_context& render_ctx);

    // Call it after linking a program whose shaders include lighting_block.glsl.
    void verify_lighting_block(const core::shader_program& program);

} // namespace
