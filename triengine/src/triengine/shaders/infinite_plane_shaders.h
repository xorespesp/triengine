#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
    // Infinite Grid Vertex Shader
    static const char* const kInfinitePlaneVertexShader = R"glsl(
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
        
		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out VS_OUT
        {
            vec3 vertPos; // vertex position in world space
            vec3 eyePos; // camera position in world-space
            float planeHalfSize; // half-size of the entire plane in world space (unit: [m])
            float gridCellSize; // plane grid cell size in world space (unit: [m])
        } vso;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix
        uniform vec3 u_eyePos; // camera position in world-space
        uniform float u_maxViewDist = 100.0; // half-size of the entire plane in world space (max view distance; unit: [m])
        uniform float u_gridCellSize = 1.0;

        void main()
        {
            const int currVertexIndex = g_vertexIndices[gl_VertexID];
            
            vec3 currVertexPos = g_vertexPositions[currVertexIndex];
            currVertexPos *= u_maxViewDist;
            currVertexPos.x += u_eyePos.x;
            currVertexPos.z += u_eyePos.z;

            gl_Position = u_proj * u_view * vec4(currVertexPos, 1.0);
            vso.vertPos = currVertexPos;
            vso.eyePos = u_eyePos;
            vso.planeHalfSize = u_maxViewDist;
            vso.gridCellSize = u_gridCellSize;
        }
    )glsl";

    // Infinite Transparent Grid Fragment Shader
    static const char* const kInfiniteTransparentGridPlaneFragmentShader = R"glsl(
        #define WBOIT_ENABLED 1

		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        in VS_OUT
        {
            vec3 vertPos; // vertex position in world space
            vec3 eyePos; // camera position in world-space
            float planeHalfSize; // half-size of the entire plane in world space (unit: [m])
            float gridCellSize; // plane grid cell size in world space (unit: [m])
        } fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        #if WBOIT_ENABLED
        layout (location = 0) out vec4 fso_accum;
        layout (location = 1) out float fso_reveal;
        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV
        out vec4 fso_fragColor;
        #endif // ^^^ !WBOIT_ENABLED ^^^

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform float u_gridMinPixelsBetweenCells = 2.0;
        uniform vec3  u_gridLineColor = vec3(0.0, 1.0, 0.0);

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
	        const float modDivX = mod_f32(fsi.vertPos.x, gridCellSize) / (thickness * ldx);
            const float gridOpacityX = 1.0 - abs(sat_f32(modDivX) * 2.0 - 1.0);

            // horizontal grid line opacity (world z-axis)
	        const float modDivZ = mod_f32(fsi.vertPos.z, gridCellSize) / (thickness * ldz);
            const float gridOpacityZ = 1.0 - abs(sat_f32(modDivZ) * 2.0 - 1.0);

            // 수직선 opacity, 수평선 opacity 값 중 더 큰 값을 grid opacity 값으로 사용
	        return max(gridOpacityX, gridOpacityZ);
        }

        // 0.0: transparent / 1.0: intransparent
        float calcGridPixelOpacity_v2(vec2 dvx, vec2 dvz, float gridCellSize)
        {
            // 거리 계산: 그리드 라인으로부터의 최소 거리
            float gridX = mod_f32(fsi.vertPos.x, gridCellSize);
            gridX = min(gridX, gridCellSize - gridX); // 그리드 라인까지의 최소 거리

            float gridZ = mod_f32(fsi.vertPos.z, gridCellSize);
            gridZ = min(gridZ, gridCellSize - gridZ); // 그리드 라인까지의 최소 거리

            // `fwidth`를 사용하여 엣지의 두께를 결정하고 부드러운 그라데이션 적용
            // NOTE: `fwidth(p)` is equivalent to `abs(dFdx(p)) + abs(dFdy(p))`
            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/fwidth.xhtml
            float lineWidthX = abs(dvx.x) + abs(dvx.y); //fwidth(fsi.vertPos.x);
            float lineWidthZ = abs(dvz.x) + abs(dvz.y); //fwidth(fsi.vertPos.z);

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
            const vec2 dvx = vec2( dFdx(fsi.vertPos.x), dFdy(fsi.vertPos.x) );
            const vec2 dvz = vec2( dFdx(fsi.vertPos.z), dFdy(fsi.vertPos.z) );

            const float ldx = length(dvx);
            const float ldz = length(dvz);

            const float LOD = max(0.0, log10_f32( length(vec2(ldx, ldz)) * u_gridMinPixelsBetweenCells / fsi.gridCellSize) + 1.0 );
            const float gridCellSize_Lod0 = fsi.gridCellSize * pow(10.0, floor(LOD));
            const float gridCellSize_Lod1 = gridCellSize_Lod0 * 10;
            const float gridCellSize_Lod2 = gridCellSize_Lod1 * 10;

            const float gridPixelOpacity_Lod0 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod0);
            const float gridPixelOpacity_Lod1 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod1);
            const float gridPixelOpacity_Lod2 = calcGridPixelOpacity_v2(dvx, dvz, gridCellSize_Lod2);

            vec4 resultColor;
            const vec4 gridLineThickColor = vec4(u_gridLineColor, 1.0);
            const vec4 gridLineThinColor = vec4(u_gridLineColor * 0.5, 1.0);
            if (gridPixelOpacity_Lod2 > 0.0) {
                resultColor = gridLineThickColor;
                resultColor.a *= gridPixelOpacity_Lod2;
            } else {
                const float LOD_fade = fract(LOD);
                if (gridPixelOpacity_Lod1 > 0.0) {
                    resultColor = mix(gridLineThickColor, gridLineThinColor, LOD_fade);
                    resultColor.a *= gridPixelOpacity_Lod1;
                } else {
                    resultColor = gridLineThinColor;
                    resultColor.a *= (gridPixelOpacity_Lod0 * (1.0 - LOD_fade));
                }
            }

            const float distanceToCamera = length(fsi.vertPos.xz - fsi.eyePos.xz);
            const float falloffOpacity = smoothstep(1.0, 0.0, sat_f32(distanceToCamera / fsi.planeHalfSize));
            resultColor.a *= falloffOpacity;

        #if WBOIT_ENABLED

            ////////////////////////////////////////////////////////////////////
            // WBOIT pass
            ////////////////////////////////////////////////////////////////////

	        const vec4 blendColor = resultColor;
            
	        // weight function
	        const float weight =
		        max(min(1.0, max(max(blendColor.r, blendColor.g), blendColor.b) * blendColor.a), blendColor.a) *
		        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
                
	        // store pixel color accumulation
	        fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	            
	        // store pixel revealage threshold
	        fso_reveal = blendColor.a;

        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV

            ////////////////////////////////////////////////////////////////////
            // Output
            ////////////////////////////////////////////////////////////////////

            fso_fragColor = resultColor;

        #endif // ^^^ !WBOIT_ENABLED ^^^

        }
    )glsl";

    // Infinite Box-filtered Grid Fragment Shader
    static const char* const kInfiniteBoxFilteredGridPlaneFragmentShader = R"glsl(
        #define WBOIT_ENABLED 1

        /**
         * Refs:
         * https://iquilezles.org/articles/filterableprocedurals/
         * https://github.com/martin-pr/possumwood/wiki/Infinite-ground-plane-using-GLSL-shaders
         */

        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT
        {
            vec3 vertPos; // vertex position in world space (보간된 값)
            vec3 eyePos; // camera position in world-space
            float planeHalfSize; // half-size of the entire plane in world space (unit: [m])
            float gridCellSize; // plane grid cell size in world space (unit: [m])
        } fsi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        #if WBOIT_ENABLED
        layout (location = 0) out vec4 fso_accum;
        layout (location = 1) out float fso_reveal;
        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV
        out vec4 fso_fragColor;
        #endif // ^^^ !WBOIT_ENABLED ^^^

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec3  u_gridLineColor = vec3(0.82, 0.82, 0.82); // 체스보드 색상 1 (vec4 형태로 RGBA)
        uniform vec3  u_gridCellColor = vec3(0.90, 0.90, 0.90); // 체스보드 색상 2 (vec4 형태로 RGBA)

        // saturation function
        float sat_f32(float x) {
            return clamp(x, 0.0, 1.0);
        }

        // Box-filtered grid by Inigo Quilez:
        // https://iquilezles.org/articles/filterableprocedurals/
        // https://www.shadertoy.com/view/XtBfzz
        vec4 box_filtered_grid(
            in vec4 grid_line_color/* 그리드 라인 색상 */, 
            in vec4 grid_cell_color/* 그리드 타일 색상 */, 
            in vec2 scaled_pos/* 스케일링된 좌표 (월드 좌표 / 셀 크기). 한 단위가 체스보드 한 칸에 해당하도록 스케일링되어야 함 */)
        {
            // 그리드 선 두께 인자. 선의 두께는 대략 (셀 크기 / N) 월드 단위.
            // 이 값이 클수록 선이 가늘어진다. (e.g: 10.0은 비교적 두꺼운 선, 50.0은 가는 선)
            // 이 값은 1.0보다 커야 의미가 있으며, 일반적으로 5.0 이상을 사용.
            const float N = 30.0;
    
            // 스케일링된 좌표의 스크린 공간 변화율(도함수) 계산
            const vec2 dpdx = dFdx(scaled_pos); // dpdx : dFdx(scaled_pos); 스크린 공간에서 scaled_pos 값의 x 변화율
            const vec2 dpdy = dFdy(scaled_pos); // dpdy : dFdy(scaled_pos); 스크린 공간에서 scaled_pos 값의 y 변화율

            // w: 픽셀의 영향 범위(필터 너비), scaled_pos 단위
            const vec2 w = max(abs(dpdx), abs(dpdy)) + 0.0001/* 매우 작은 epsilon 추가 (N*w가 0이 되지 않도록 해서 0으로 나누기 방지) */;

            // a, b: 필터 박스의 경계
            const vec2 a = scaled_pos + 0.5*w;                        
            const vec2 b = scaled_pos - 0.5*w;
    
            // i: 각 축에 대한 "선"의 커버리지.
            // floor(x) + min(fract(x)*N, 1.0)은 부드러운 스텝 함수를 만듦.
            // 선의 폭이 1/N이 되도록 함.
            const vec2 i = (floor(a)+min(fract(a)*N,1.0)-
                            floor(b)-min(fract(b)*N,1.0))/(N*w);
    
            // (1.0-i.x): 수직선이 아닌 부분의 강도 (셀 강도 x축)
            // (1.0-i.y): 수평선이 아닌 부분의 강도 (셀 강도 y축)
            // 곱하면 셀 내부 영역의 전체적인 강도가 됨.
            // 즉 scaled_pos가 셀 내부일 경우 ~1.0, 선일 경우 ~0.0
            const float grid_intensity = (1.0-i.x) * (1.0-i.y);
    
            // 4. 최종 색상 결정: 선 색상과 셀 배경색을 grid_intensity를 기준으로 혼합
            // grid_intensity가 1이면 u_cell_bg_color, 0이면 u_line_color가 됨.
            return mix(grid_line_color, grid_cell_color, grid_intensity);
        } 

        void main()
        {
            // 그리드 함수에 사용할 스케일링된 좌표 계산
            // (fsi.vertPos.xz는 월드 좌표. fsi.gridCellSize로 나누어 그리드 선이 정수 좌표에 오도록 스케일링)
            const vec2 scaledPos = fsi.vertPos.xz / fsi.gridCellSize;
    
            // Inigo Quilez의 함수를 사용하여 필터링된 그리드 라인/셀 색상 계산
            vec4 resultColor = box_filtered_grid(
                vec4(u_gridLineColor, 0.85),
                vec4(u_gridCellColor, 0.85),
                scaledPos);

            // 카메라로부터의 거리에 따른 페이드 아웃 처리
            const float distanceToCenterOfQuad = length(fsi.vertPos.xz - fsi.eyePos.xz);
            const float normalizedDistance = sat_f32(distanceToCenterOfQuad / fsi.planeHalfSize);
            const float falloffOpacity = smoothstep(1.0, 0.0, normalizedDistance);
            resultColor.a *= falloffOpacity; // 그리드 함수에서 계산된 알파 값에 falloffOpacity 적용

            // 결과 색상이 완전히 투명한 경우는 폐기 (optional)
            if (resultColor.a < 0.01) {
                discard;
            }

            // 계산된 색상 (RGBA)을 최종 출력 색상으로 설정...
        #if WBOIT_ENABLED

            ////////////////////////////////////////////////////////////////////
            // WBOIT pass
            ////////////////////////////////////////////////////////////////////

	        const vec4 blendColor = resultColor;
            
	        // weight function
	        const float weight =
		        max(min(1.0, max(max(blendColor.r, blendColor.g), blendColor.b) * blendColor.a), blendColor.a) *
		        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
                
	        // store pixel color accumulation
	        fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	            
	        // store pixel revealage threshold
	        fso_reveal = blendColor.a;

        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV

            ////////////////////////////////////////////////////////////////////
            // Output
            ////////////////////////////////////////////////////////////////////

            fso_fragColor = resultColor;

        #endif // ^^^ !WBOIT_ENABLED ^^^
        }
    )glsl";

    // Infinite Box-filtered Chess Grid Fragment Shader
    static const char* const kInfiniteBoxFilteredChessPlaneFragmentShader = R"glsl(
        #define WBOIT_ENABLED 1

        /**
         * Refs:
         * https://iquilezles.org/articles/filterableprocedurals/
         * https://github.com/martin-pr/possumwood/wiki/Infinite-ground-plane-using-GLSL-shaders
         */

        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT
        {
            vec3 vertPos; // vertex position in world space (보간된 값)
            vec3 eyePos; // camera position in world-space
            float planeHalfSize; // half-size of the entire plane in world space (unit: [m])
            float gridCellSize; // plane grid cell size in world space (unit: [m])
        } fsi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        #if WBOIT_ENABLED
        layout (location = 0) out vec4 fso_accum;
        layout (location = 1) out float fso_reveal;
        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV
        out vec4 fso_fragColor;
        #endif // ^^^ !WBOIT_ENABLED ^^^

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec3  u_gridCellColor1 = vec3(0.82, 0.82, 0.82); // 체스보드 색상 1 (vec4 형태로 RGBA)
        uniform vec3  u_gridCellColor2 = vec3(0.90, 0.90, 0.90); // 체스보드 색상 2 (vec4 형태로 RGBA)

        // saturation function
        float sat_f32(float x) {
            return clamp(x, 0.0, 1.0);
        }

        // Box-filtered chessboard by Inigo Quilez:
        // https://iquilezles.org/articles/filterableprocedurals/
        // https://www.shadertoy.com/view/XlcSz2
        vec4 box_filtered_chessboard(
            in vec4 c1/* 타일 색상1 */, 
            in vec4 c2/* 타일 색상2 */, 
            in vec2 scaled_pos/* 스케일링된 좌표 (월드 좌표 / 셀 크기). 한 단위가 체스보드 한 칸에 해당하도록 스케일링되어야 함 */)
        {
            // 스케일링된 좌표의 스크린 공간 변화율(도함수) 계산
            const vec2 dpdx = dFdx(scaled_pos); // dpdx : dFdx(scaled_pos); 스크린 공간에서 scaled_pos 값의 x 변화율
            const vec2 dpdy = dFdy(scaled_pos); // dpdy : dFdy(scaled_pos); 스크린 공간에서 scaled_pos 값의 y 변화율

            // w : 픽셀의 영향 범위(필터 너비).
            const vec2 w = max(abs(dpdx), abs(dpdy)) + 0.0001/* 매우 작은 epsilon 추가 (0으로 나누기 방지) */;

            // i는 각 차원에서 현재 픽셀이 얼마나 특정 색상 영역에 걸쳐 있는지 계산.
            // (scaled_pos - 0.5 * w) / 2.0 와 (scaled_pos + 0.5 * w) / 2.0 는 필터 박스의 경계를 나타냄.
            // fract 함수와 abs 함수 조합은 삼각파 형태를 만들며, 이를 통해 부드러운 전환을 구현.
            const vec2 i = 2.0 * (abs(fract((scaled_pos - 0.5 * w) / 2.0) - 0.5) - 
                                  abs(fract((scaled_pos + 0.5 * w) / 2.0) - 0.5)) / w;

            // weight는 두 색상을 혼합할 비율.
            // i.x * i.y 값에 따라 weight가 0에서 1 사이의 값을 가지게 됨.
            // weight가 0이면 c1, 1이면 c2, 0.5면 두 색상의 중간.
            const float weight = 0.5 - 0.5 * i.x * i.y;

            return mix(c1, c2, weight); // 계산된 weight에 따라 c1과 c2를 혼합.
        }

        void main()
        {
            // 체스보드 함수에 사용할 스케일링된 좌표 계산
            // (fsi.vertPos.xz는 월드 좌표. fsi.gridCellSize로 나누어 한 칸이 1.0x1.0 크기가 되도록 스케일링)
            const vec2 scaledPos = fsi.vertPos.xz / fsi.gridCellSize;

            // Inigo Quilez의 함수를 사용하여 필터링된 체스보드 색상 계산
            vec4 resultColor = box_filtered_chessboard(
                vec4(u_gridCellColor1, 0.85), 
                vec4(u_gridCellColor2, 0.85), 
                scaledPos);

            // 카메라로부터의 거리에 따른 페이드 아웃 처리
            const float distanceToCenterOfQuad = length(fsi.vertPos.xz - fsi.eyePos.xz);
            const float normalizedDistance = sat_f32(distanceToCenterOfQuad / fsi.planeHalfSize);
            const float falloffOpacity = smoothstep(1.0, 0.0, normalizedDistance);
            resultColor.a *= falloffOpacity; // 체스보드 함수에서 계산된 알파 값에 falloffOpacity 적용
    
            // 결과 색상이 완전히 투명한 경우는 폐기 (optional)
            if (resultColor.a < 0.01) {
                discard;
            }

            // 계산된 색상 (RGBA)을 최종 출력 색상으로 설정...
        #if WBOIT_ENABLED

            ////////////////////////////////////////////////////////////////////
            // WBOIT pass
            ////////////////////////////////////////////////////////////////////

	        const vec4 blendColor = resultColor;
            
	        // weight function
	        const float weight =
		        max(min(1.0, max(max(blendColor.r, blendColor.g), blendColor.b) * blendColor.a), blendColor.a) *
		        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
                
	        // store pixel color accumulation
	        fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	            
	        // store pixel revealage threshold
	        fso_reveal = blendColor.a;

        #else  // ^^^ WBOIT_ENABLED ^^^ / VVV !WBOIT_ENABLED VVV

            ////////////////////////////////////////////////////////////////////
            // Output
            ////////////////////////////////////////////////////////////////////

            fso_fragColor = resultColor;

        #endif // ^^^ !WBOIT_ENABLED ^^^
        }
    )glsl";

} // namespace