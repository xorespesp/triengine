// Texture-Shaded Mesh Object Vertex Shader (deferred geometry pass)

#include "includes/phong_material.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec2 texCoords; // texture uv coordinate
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec3 fso_gBufferFragPos;    // [0]: position buffer; fragPos(RGB)
layout (location = 1) out vec3 fso_gBufferNormal;     // [1]: normal buffer; fragNormal(RGB)
layout (location = 2) out vec4 fso_gBufferAlbedoSpec; // [2]: color buffer; albedoColor(RGB), specularColor(A)
layout (location = 3) out vec4 fso_gBufferMaterial;   // [3]: material buffer; ambientIntensity(R), diffuseIntensity(G), specularIntensity(B), shininess(A)

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- materials ---
layout(binding = 0) uniform sampler2D u_diffuseMap; // diffuse color map (RGB)
layout(binding = 1) uniform sampler2D u_specularMap; // specular color map (grayscale)
uniform PhongShadedObjectMaterial u_phongMaterial;

void main()
{
    // Position
    fso_gBufferFragPos = fsi.fragPosInView;

    // Normal
    fso_gBufferNormal = normalize(fsi.fragNormalInView);
    
    // Albedo and specular color
    fso_gBufferAlbedoSpec = vec4(
        texture(u_diffuseMap, fsi.texCoords).rgb, // albedo(diffuse) color
        texture(u_specularMap, fsi.texCoords).r // specular color
    );

    // Materials
    fso_gBufferMaterial = vec4(
        u_phongMaterial.ambientIntensity, 
        u_phongMaterial.diffuseIntensity, 
        u_phongMaterial.specularIntensity, 
        u_phongMaterial.shininess
    );
}