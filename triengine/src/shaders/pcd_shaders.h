#pragma once
#include "shader_version.h"

namespace triengine::shaders
{
    // Pointcloud Object Vertex Shader
    static const char* const kPcdVertexShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
        layout(location = 1) in vec3 vsi_vertNormal; // object-space vertex normal
        layout(location = 2) in vec3 vsi_vertColor; // vertex color

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out VS_OUT
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } vso;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix
        uniform mat3 u_nmv; // normal matrix in view-space; `mat3(transpose(inverse(u_view * u_model)))`

        void main()
        {
            vso.view = u_view;
            vso.fragPosInView = vec3(u_view * u_model * vec4(vsi_vertPos, 1.0));
            vso.fragNormalInView = u_nmv * vsi_vertNormal;
			vso.fragColor = vsi_vertColor;

            gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
        }
    )glsl";


    // Pointcloud Object Fragment Shader (for solid object rendering)
    static const char* const kSolidPcdFragmentShader = R"glsl(
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
    )glsl";


    // Pointcloud Object Fragment Shader (for transparent object rendering)
    static const char* const kTransparentPcdFragmentShader = R"glsl(
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
        layout (location = 0) out vec4 fso_accum;
        layout (location = 1) out float fso_reveal;

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
            // WBOIT pass
            ////////////////////////////////////////////////////////////////////

	        const vec4 blendColor = vec4(resultColor, u_material.alpha);
            
	        // weight function
	        const float weight =
		        max(min(1.0, max(max(blendColor.r, blendColor.g), blendColor.b) * blendColor.a), blendColor.a) *
		        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
                
	        // store pixel color accumulation
	        fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	            
	        // store pixel revealage threshold
	        fso_reveal = blendColor.a;
        }
    )glsl";

} // namespace
