// Vertex-Shaded Mesh Object Fragment Shader (deferred geometry pass)

#include "includes/phong_material.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec3 fragColor; // fragment color
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
uniform PhongShadedObjectMaterial u_phongMaterial;

void main()
{
    // Position
    fso_gBufferFragPos = fsi.fragPosInView;

    // Normal
    fso_gBufferNormal = normalize(fsi.fragNormalInView);
    
    // Albedo and specular color
    fso_gBufferAlbedoSpec = vec4(
        fsi.fragColor.rgb, // albedo(diffuse) color
        1.0 // specular color
    );

    // Materials
    fso_gBufferMaterial = vec4(
        u_phongMaterial.ambientIntensity, 
        u_phongMaterial.diffuseIntensity, 
        u_phongMaterial.specularIntensity, 
        u_phongMaterial.shininess
    );
}