// WBOIT weight function
float computeWBOITWeight(
    vec4 blendColor,
    float viewSpaceDist/* linear eye-space distance, e.g. -fragPosInView.z */)
{
    // 1. 밝기 기반 색상 가중치
    const float maxRGB = max(max(blendColor.r, blendColor.g), blendColor.b);
    const float colorWeight = max(min(1.0, maxRGB * blendColor.a), blendColor.a);

    // 2. 깊이 기반 거리 감쇠
    // NOTE: McGuire's WBOIT weight is defined in terms of linear eye-space distance (not the
    //       non-linear gl_FragCoord.z), so this works correctly for both perspective and
    //       orthographic projections.
    const float depthTerm = pow(viewSpaceDist / 200.0, 4.0);
    const float depthWeight = clamp(0.03 / (1e-5 + depthTerm), 1e-2, 3e3);

    // 3. 최종 가중치 계산
    return colorWeight * depthWeight;
}