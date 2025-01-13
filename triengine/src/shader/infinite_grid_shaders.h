#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    // Infinite Grid Vertex Shader
    static const char* const kInfiniteGridVertexShader = R"(
        out VertOut
        {
            vec3 vertPos; // vertex position in world space
            vec3 eyePos; // camera position in world-space
            float gridSize; // total grid size (unit: [m])
        } vo;

        const vec3 g_vertexPositions[4] = vec3[4](
	        vec3(-1.0, 0.0, -1.0), // [0]: bottom left
	        vec3( 1.0, 0.0, -1.0), // [1]: bottom right
	        vec3( 1.0, 0.0,  1.0), // [2]: top right
	        vec3(-1.0, 0.0,  1.0)  // [3]: top left
        );

        const int g_vertexIndices[6] = int[6](
	        0, 2, 1, // first triangle 
	        2, 0, 3  // second triangle
        );
        
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix
        uniform vec3 u_eyePos; // camera position in world-space
        uniform float u_gridSize = 100.0; // total grid size (unit: [m])

        void main()
        {
            const int currVertexIndex = g_vertexIndices[gl_VertexID];
            
            vec3 currVertexPos = g_vertexPositions[currVertexIndex];
            currVertexPos *= u_gridSize;
            currVertexPos.x += u_eyePos.x;
            currVertexPos.z += u_eyePos.z;

            gl_Position = u_proj * u_view * vec4(currVertexPos, 1.0);
            vo.vertPos = currVertexPos;
            vo.eyePos = u_eyePos;
            vo.gridSize = u_gridSize;
        }
    )";

    // Infinite Grid Fragment Shader
    static const char* const kInfiniteGridFragmentShader = R"(
        in VertOut
        {
            vec3 vertPos; // vertex position in world space
            vec3 eyePos; // camera position in world-space
            float gridSize; // total grid size (unit: [m])
        } fi;

        uniform float u_gridMinPixelsBetweenCells = 2.0;
        uniform float u_gridCellSize = 1.0;
        uniform vec3  u_gridColor = vec3(0.0, 1.0, 0.0);

        out vec4 fo_fragColor;

        // modulo function; returns the value of x modulo y.
        // (equivalent of: https://registry.khronos.org/OpenGL-Refpages/gl4/html/mod.xhtml)
        float mod_f32(float x, float y) {
            return x - y * floor(x/y);
        }

        // saturation function
        float sat_f32(float x) {
            return clamp(x, 0.0, 1.0);
        }

        // base10 log function
        float log10_f32(float x) {
            return log(x) / log(10.0);
        }

        // 0.0: transparent / 1.0: intransparent
        float calcGridPixelOpacity_v1(float ldx, float ldz, float gridCellSize)
        {
	        const float thickness = 2.0; // grid line thickness

            // vertical grid line opacity (world x-axis)
	        const float modDivX = mod_f32(fi.vertPos.x, gridCellSize) / (thickness * ldx);
            const float gridOpacityX = 1.0 - abs(sat_f32(modDivX) * 2.0 - 1.0);

            // horizontal grid line opacity (world z-axis)
	        const float modDivZ = mod_f32(fi.vertPos.z, gridCellSize) / (thickness * ldz);
            const float gridOpacityZ = 1.0 - abs(sat_f32(modDivZ) * 2.0 - 1.0);

            // 수직선 opacity, 수평선 opacity 값 중 더 큰 값을 grid opacity 값으로 사용
	        return max(gridOpacityX, gridOpacityZ);
        }

        // 0.0: transparent / 1.0: intransparent
        float calcGridPixelOpacity_v2(vec2 dvx, vec2 dvz, float gridCellSize)
        {
            // 거리 계산: 그리드 라인으로부터의 최소 거리
            float gridX = mod_f32(fi.vertPos.x, gridCellSize);
            gridX = min(gridX, gridCellSize - gridX); // 그리드 라인까지의 최소 거리

            float gridZ = mod_f32(fi.vertPos.z, gridCellSize);
            gridZ = min(gridZ, gridCellSize - gridZ); // 그리드 라인까지의 최소 거리

            // `fwidth`를 사용하여 엣지의 두께를 결정하고 부드러운 그라데이션 적용
            // NOTE: `fwidth(p)` is equivalent to `abs(dFdx(p)) + abs(dFdy(p))`
            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/fwidth.xhtml
            float lineWidthX = abs(dvx.x) + abs(dvx.y); //fwidth(fi.vertPos.x);
            float lineWidthZ = abs(dvz.x) + abs(dvz.y); //fwidth(fi.vertPos.z);

            // line의 두께 조절
            const float thickness = 1.25; // grid line thickness
            lineWidthX *= thickness;
            lineWidthZ *= thickness;

            // 그리드 라인 근처에서 알파값을 부드럽게 변경
            float alphaX = 1.0 - smoothstep(0.0, lineWidthX, gridX);
            float alphaZ = 1.0 - smoothstep(0.0, lineWidthZ, gridZ);

            // 수직 및 수평 그리드 라인 중 더 두꺼운 부분을 선택
            return max(alphaX, alphaZ);
        }

        void main()
        {
            const vec2 dvx = vec2( dFdx(fi.vertPos.x), dFdy(fi.vertPos.x) );
            const vec2 dvz = vec2( dFdx(fi.vertPos.z), dFdy(fi.vertPos.z) );

            const float ldx = length(dvx);
            const float ldz = length(dvz);

            const float LOD = max(0.0, log10_f32( length(vec2(ldx, ldz)) * u_gridMinPixelsBetweenCells / u_gridCellSize) + 1.0 );
            const float gridCellSize_Lod0 = u_gridCellSize * pow(10.0, floor(LOD));
            const float gridCellSize_Lod1 = gridCellSize_Lod0 * 10;
            const float gridCellSize_Lod2 = gridCellSize_Lod1 * 10;

            const float gridPixelOpacity_Lod0 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod0);
            const float gridPixelOpacity_Lod1 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod1);
            const float gridPixelOpacity_Lod2 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod2);

            vec4 resultColor;
            const vec4 gridThickColor = vec4(u_gridColor, 1.0);
            const vec4 gridThinColor = vec4(u_gridColor * 0.5, 1.0);
            if (gridPixelOpacity_Lod2 > 0.0) {
                resultColor = gridThickColor;
                resultColor.a *= gridPixelOpacity_Lod2;
            } else {
                const float LOD_fade = fract(LOD);
                if (gridPixelOpacity_Lod1 > 0.0) {
                    resultColor = mix(gridThickColor, gridThinColor, LOD_fade);
                    resultColor.a *= gridPixelOpacity_Lod1;
                } else {
                    resultColor = gridThinColor;
                    resultColor.a *= (gridPixelOpacity_Lod0 * (1.0 - LOD_fade));
                }
            }

            const float distanceToCamera = length(fi.vertPos.xz - fi.eyePos.xz);
            const float falloffOpacity = smoothstep(1.0, 0.0, sat_f32(distanceToCamera / fi.gridSize));
            resultColor.a *= falloffOpacity;

	        fo_fragColor = resultColor;
        }
    )";

} // namespace