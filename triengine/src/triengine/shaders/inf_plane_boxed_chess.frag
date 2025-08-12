// Infinite Box-filtered Chess Grid Fragment Shader

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