#pragma once

namespace hg::render::gl33 {

/// <summary>
/// Shared GLSL 330 Gerstner-wave implementation used by both the water mesh
/// and particles that must intersect the actual displaced surface.
/// </summary>
inline constexpr char kWorld11WaterWavesGlsl[] = R"glsl(
const float WORLD11_PI2 = 6.28318530718;
const float WORLD11_WATER_BASE_Y = 12.5;

void world11AddWaterWave(inout vec3 position, vec2 point, vec2 direction,
                         float amplitude, float wavelength, float speed,
                         float steepness, vec2 source, float phaseOffset) {
  vec2 local = point - source;
  float distanceFromSource = length(local);
  float travel = dot(direction, local);
  float waveNumber = WORLD11_PI2 / wavelength;
  float angularFrequency = speed * waveNumber;
  float amplitudePulse = 0.82 + 0.18 *
      sin(distanceFromSource * 0.12 -
          angularFrequency * uTime * 0.38 + phaseOffset);
  float propagatedAmplitude = amplitude * amplitudePulse;
  float phase = waveNumber * travel - angularFrequency * uTime + phaseOffset;
  float horizontal = steepness * propagatedAmplitude * cos(phase);
  position.xz += direction * horizontal;
  position.y += propagatedAmplitude * sin(phase);
}

vec3 world11WaterDisplacedPosition(vec2 point) {
  vec3 position = vec3(point.x, WORLD11_WATER_BASE_Y, point.y);
  world11AddWaterWave(position, point, vec2(0.8944, 0.4472),
      0.42, 13.0, 1.55, 0.42, vec2(-34.0, 18.0), 0.2);
  world11AddWaterWave(position, point, vec2(-0.3511, 0.9363),
      0.27, 8.5, 1.15, 0.30, vec2(28.0, 20.0), 1.7);
  world11AddWaterWave(position, point, vec2(0.1961, -0.9806),
      0.18, 5.2, 0.82, 0.22, vec2(-12.0, -74.0), 3.1);
  world11AddWaterWave(position, point, vec2(-0.7809, -0.6247),
      0.12, 3.4, 0.58, 0.16, vec2(35.0, -58.0), 4.4);
  return position;
}

vec3 world11WaterSurfaceAtWorldXZ(vec2 worldXZ) {
  vec2 parameterPoint = worldXZ;
  for (int iteration = 0; iteration < 3; ++iteration) {
    vec3 displaced = world11WaterDisplacedPosition(parameterPoint);
    parameterPoint += worldXZ - displaced.xz;
  }
  return world11WaterDisplacedPosition(parameterPoint);
}
)glsl";

}  // namespace hg::render::gl33
