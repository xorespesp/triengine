// Pointcloud Object Fragment Shader (for opaque object rendering pass)

#define USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING 1
#include <phong_lighting>

struct PcdMaterial
{
    float ambient; // ambient intensity
    float diffuse; // diffuse intensity
    float specular; // specular intensity
    float shininess; // surface shininess scalar (>= 0)
    float alpha; // object transparency (WBOIT)
};

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    mat4 view; // view matrix (for view-space light calculation)
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec3 fragColor; // fragment color
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform DirLight u_dirLight;
uniform PointLight u_pointLight;
uniform PcdMaterial u_material;

void main()
{
    ////////////////////////////////////////////////////////////////////
    // Lighting pass
    ////////////////////////////////////////////////////////////////////

    vec3 resultColor = vec3(0.0);
    if (u_dirLight.enabled || u_pointLight.enabled)
    {
        const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
        const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction;
                                                                // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`

        // 1) apply directional light
        if (u_dirLight.enabled) {
            resultColor += calcDirLightInViewSpace(
                fsi.view,
                eyeDirInView,
                fragNormalInView, 
                u_dirLight,
                vec3(u_material.ambient),
                vec3(u_material.diffuse),
                vec3(u_material.specular),
                u_material.shininess 
            );
        }

        // 2) apply point light
        if (u_pointLight.enabled) {
            resultColor += calcPointLightInViewSpace(
                fsi.view,
                eyeDirInView,
                fsi.fragPosInView,
                fragNormalInView, 
                u_pointLight,
                vec3(u_material.ambient),
                vec3(u_material.diffuse),
                vec3(u_material.specular),
                u_material.shininess 
            );
        }

        resultColor *= fsi.fragColor.rgb;
    }
    else
    {
        resultColor = fsi.fragColor.rgb;
    }

    ////////////////////////////////////////////////////////////////////
    // Output
    ////////////////////////////////////////////////////////////////////

    fso_fragColor = vec4(resultColor, 1.0/* alpha */);
}