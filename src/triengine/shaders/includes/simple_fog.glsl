/**
 * Simple Fog Equations (Fog Factor Calculations)
 */

 // Linear fog
float simpleFogLinear(
    float dist_to_camera,
    float fog_min_dist,
    float fog_max_dist)
{
    const float fog_factor = (fog_max_dist - dist_to_camera) / (fog_max_dist - fog_min_dist);
    return clamp(fog_factor, 0.0, 1.0);
}

// Exponential fog
float simpleFogExp(
    float dist_to_camera,
    float fog_density)
{
    const float fog_factor = exp(-fog_density * dist_to_camera);
    return clamp(fog_factor, 0.0, 1.0);
}

// Squared exponential fog
float simpleFogExp2(
    float dist_to_camera,
    float fog_density)
{
    const float exponent = fog_density * dist_to_camera;
    const float fog_factor = exp(-exponent * exponent);
    return clamp(fog_factor, 0.0, 1.0);
}

// Squared exponential fog, with minimum start distance
float simpleFogExp2WithMinDist(
    float dist_to_camera,
    float fog_density,
    float fog_min_dist)
{
    // 안개가 시작되는 '유효 거리' 계산
    // 즉, 이 '유효 거리'는 dist_to_camera가 fog_min_dist보다 작을 때는 항상 0.0(안개 없음)으로 처리된다.
    const float effective_dist = max(dist_to_camera - fog_min_dist, 0.0);

    // 이 유효 거리를 사용해 exp2 안개 계수 계산
    const float exponent = fog_density * effective_dist;
    const float fog_factor = exp(-exponent * exponent);

    return clamp(fog_factor, 0.0, 1.0);
}

struct SimpleFogOptions
{
    bool enabled;
    vec3 color; // fog color
    float density; // fog density (0.0: no fog, higher value: den
    float startDist; // distance at which fog starts (from the camera)
};