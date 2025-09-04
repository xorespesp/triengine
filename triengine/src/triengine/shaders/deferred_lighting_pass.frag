// Deferred Lighting Pass Fragment Shader

#define USE_BLINN_PHONG_SHADING 1
//#define USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING 1
#include "includes/phong_lighting.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec4 fso_frag;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- gbuffers ---
layout(binding = 0) uniform sampler2D u_gBufferFragPos;    // [0]: position buffer; fragPos(RGB)
layout(binding = 1) uniform sampler2D u_gBufferNormal;     // [1]: normal buffer; fragNormal(RGB)
layout(binding = 2) uniform sampler2D u_gBufferAlbedoSpec; // [2]: color buffer; albedoColor(RGB), specularColor(A)
layout(binding = 3) uniform sampler2D u_gBufferMaterial;   // [3]: material buffer; ambientIntensity(R), diffuseIntensity(G), specularIntensity(B), shininess(A)

// --- lights ---
uniform DirLight u_dirLight;
uniform PointLight u_pointLight;

void main()
{
    const vec3 fragPosInView = texture(u_gBufferFragPos, fsi.texCoord).rgb;
    const vec3 eyeDirInView = normalize(-fragPosInView); // view-space eye(view) direction;
                                                         // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`
    const vec3 fragNormalInView = texture(u_gBufferNormal, fsi.texCoord).rgb;
    const vec4 albedoSpecColor = texture(u_gBufferAlbedoSpec, fsi.texCoord).rgba; // fragment color(albedo color) + specular color
    
    const vec4 materialInfo = texture(u_gBufferMaterial, fsi.texCoord).rgba;
    const float ambientIntensity = materialInfo.r; // ambient intensity
    const float diffuseIntensity = materialInfo.g; // diffuse intensity
    const float specularIntensity = materialInfo.b; // specular intensity
    const float shininess = materialInfo.a; // object surface shininess scalar (must be `> 0`)

    const vec3 ambientColor = albedoSpecColor.rgb * ambientIntensity;
    const vec3 diffuseColor = albedoSpecColor.rgb * diffuseIntensity;
    const vec3 specularColor = vec3(albedoSpecColor.a) * specularIntensity;

    vec3 resultColor = vec3(0.0);
    
    // Apply directional light
    if (u_dirLight.enabled) {
        resultColor += calcDirLightInViewSpace(
            eyeDirInView,
            fragNormalInView, 
            u_dirLight,
            ambientColor,
            diffuseColor,
            specularColor,
            shininess 
        );
    }

    // Apply point light
    if (u_pointLight.enabled) {
        resultColor += calcPointLightInViewSpace(
            eyeDirInView,
            fragPosInView,
            fragNormalInView, 
            u_pointLight,
            ambientColor,
            diffuseColor,
            specularColor,
            shininess
        );
    }
    
    // If no any lights, fallback to base color
    if (!u_dirLight.enabled && !u_pointLight.enabled) {
        resultColor = albedoSpecColor.rgb; // albedo color
    }

	fso_frag = vec4(resultColor, 1.0f);
}