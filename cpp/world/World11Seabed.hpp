#pragma once

#include <cmath>

namespace hg::world {

namespace detail {

inline float world11GaussianMound(float x, float z, float centerX,
                                  float centerZ, float radius,
                                  float amplitude) {
  const float offsetX = (x - centerX) / radius;
  const float offsetZ = (z - centerZ) / radius;
  return amplitude * std::exp(-(offsetX * offsetX + offsetZ * offsetZ));
}

}  // namespace detail

/// <summary>
/// Deterministic World 11 seabed used by rendering, collision and decoration placement.
/// Keep its paired GLSL implementation below mathematically identical.
/// </summary>
inline float world11SeabedHeight(float x, float z) {
  const float warpedX = x + std::sin(z * 0.021f + 0.7f) * 11.0f +
      std::sin(x * 0.013f - z * 0.017f) * 5.0f;
  const float warpedZ = z + std::sin(x * 0.018f - 1.2f) * 13.0f +
      std::cos(z * 0.011f + x * 0.009f) * 7.0f;

  const float broadRelief =
      2.35f * std::sin(warpedX * 0.031f + warpedZ * 0.020f + 0.4f) +
      1.70f * std::sin(-warpedX * 0.018f + warpedZ * 0.034f + 1.8f) +
      1.15f * std::cos(warpedX * 0.041f - warpedZ * 0.009f - 0.6f);
  const float wideDunes =
      0.85f * std::sin(warpedX * 0.105f + warpedZ * 0.038f) +
      0.55f * std::sin(-warpedX * 0.061f + warpedZ * 0.083f + 2.4f);

  const float localRelief =
      detail::world11GaussianMound(x, z, 52.0f, -96.0f, 34.0f, 3.8f) +
      detail::world11GaussianMound(x, z, -68.0f, -148.0f, 42.0f, 3.1f) -
      detail::world11GaussianMound(x, z, -35.0f, -62.0f, 46.0f, 4.2f) -
      detail::world11GaussianMound(x, z, 110.0f, 45.0f, 55.0f, 3.4f) +
      detail::world11GaussianMound(x, z, 0.0f, -20.0f, 24.0f, 4.1f);

  const float ripplePhase =
      (x * 0.92f + z * 0.28f) * 1.48f + std::sin(z * 0.12f) * 0.38f;
  const float sandRipples =
      std::sin(ripplePhase) * 0.035f +
      std::sin(ripplePhase * 2.0f + 0.65f) * 0.012f +
      std::sin(x * 0.36f - z * 0.51f + 2.2f) * 0.010f;

  return -6.0f + broadRelief + wideDunes + localRelief + sandRipples;
}

/// <summary>GLSL 330 counterpart of <see cref="world11SeabedHeight"/>.</summary>
inline constexpr char kWorld11SeabedGlsl[] = R"glsl(
float world11GaussianMound(vec2 point, vec2 center, float radius,
                           float amplitude) {
  vec2 offset = (point - center) / radius;
  return amplitude * exp(-(offset.x * offset.x + offset.y * offset.y));
}

float seabedHeight(vec2 point) {
  float x = point.x;
  float z = point.y;
  float warpedX = x + sin(z * 0.021 + 0.7) * 11.0 +
      sin(x * 0.013 - z * 0.017) * 5.0;
  float warpedZ = z + sin(x * 0.018 - 1.2) * 13.0 +
      cos(z * 0.011 + x * 0.009) * 7.0;

  float broadRelief =
      2.35 * sin(warpedX * 0.031 + warpedZ * 0.020 + 0.4) +
      1.70 * sin(-warpedX * 0.018 + warpedZ * 0.034 + 1.8) +
      1.15 * cos(warpedX * 0.041 - warpedZ * 0.009 - 0.6);
  float wideDunes =
      0.85 * sin(warpedX * 0.105 + warpedZ * 0.038) +
      0.55 * sin(-warpedX * 0.061 + warpedZ * 0.083 + 2.4);

  float localRelief =
      world11GaussianMound(point, vec2(52.0, -96.0), 34.0, 3.8) +
      world11GaussianMound(point, vec2(-68.0, -148.0), 42.0, 3.1) -
      world11GaussianMound(point, vec2(-35.0, -62.0), 46.0, 4.2) -
      world11GaussianMound(point, vec2(110.0, 45.0), 55.0, 3.4) +
      world11GaussianMound(point, vec2(0.0, -20.0), 24.0, 4.1);

  float ripplePhase =
      (x * 0.92 + z * 0.28) * 1.48 + sin(z * 0.12) * 0.38;
  float sandRipples =
      sin(ripplePhase) * 0.035 +
      sin(ripplePhase * 2.0 + 0.65) * 0.012 +
      sin(x * 0.36 - z * 0.51 + 2.2) * 0.010;

  return -6.0 + broadRelief + wideDunes + localRelief + sandRipples;
}
)glsl";

}  // namespace hg::world
