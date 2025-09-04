// WBOIT weight function
float computeWBOITWeight(
    vec4 blendColor, 
    float fragDepth/* gl_FragCoord.z */)
{
    // 1. 밝기 기반 색상 가중치
    const float maxRGB = max(max(blendColor.r, blendColor.g), blendColor.b);
    const float colorWeight = max(min(1.0, maxRGB * blendColor.a), blendColor.a);

    // 2. 깊이 기반 거리 감쇠
    const float depthTerm = pow(fragDepth / 200.0, 4.0);
    const float depthWeight = clamp(0.03 / (1e-5 + depthTerm), 1e-2, 3e3);

    // 3. 최종 가중치 계산
    return colorWeight * depthWeight;
}