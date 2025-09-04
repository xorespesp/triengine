// Infinite Box-filtered Grid Fragment Shader

#define WBOIT_ENABLED 1
#include "includes/WBOIT.glsl"

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

	// calculate weight
    const float weight = computeWBOITWeight(blendColor, gl_FragCoord.z);
                
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