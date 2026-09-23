#pragma once

#include <cmath>

namespace hg::world {

inline constexpr float kWorld11WaterBaseY = 12.5f;

namespace detail {

struct World11WaterPoint {
  float x;
  float y;
  float z;
};

inline void addWorld11WaterWave(
    World11WaterPoint& position, float pointX, float pointZ,
    float directionX, float directionZ, float amplitude, float wavelength,
    float speed, float steepness, float sourceX, float sourceZ,
    float phaseOffset, float simulationTime) {
  constexpr float kTwoPi = 6.28318530718f;
  const float localX = pointX - sourceX;
  const float localZ = pointZ - sourceZ;
  const float distanceFromSource = std::sqrt(
      localX * localX + localZ * localZ);
  const float travel = directionX * localX + directionZ * localZ;
  const float waveNumber = kTwoPi / wavelength;
  const float angularFrequency = speed * waveNumber;
  const float amplitudePulse = 0.82f + 0.18f * std::sin(
      distanceFromSource * 0.12f -
      angularFrequency * simulationTime * 0.38f + phaseOffset);
  const float propagatedAmplitude = amplitude * amplitudePulse;
  const float phase = waveNumber * travel -
      angularFrequency * simulationTime + phaseOffset;
  const float horizontal = steepness * propagatedAmplitude * std::cos(phase);
  position.x += directionX * horizontal;
  position.z += directionZ * horizontal;
  position.y += propagatedAmplitude * std::sin(phase);
}

inline World11WaterPoint world11WaterDisplacedPosition(
    float pointX, float pointZ, float simulationTime) {
  World11WaterPoint position{pointX, kWorld11WaterBaseY, pointZ};
  addWorld11WaterWave(position, pointX, pointZ, 0.8944f, 0.4472f,
      0.42f, 13.0f, 1.55f, 0.42f, -34.0f, 18.0f, 0.2f,
      simulationTime);
  addWorld11WaterWave(position, pointX, pointZ, -0.3511f, 0.9363f,
      0.27f, 8.5f, 1.15f, 0.30f, 28.0f, 20.0f, 1.7f,
      simulationTime);
  addWorld11WaterWave(position, pointX, pointZ, 0.1961f, -0.9806f,
      0.18f, 5.2f, 0.82f, 0.22f, -12.0f, -74.0f, 3.1f,
      simulationTime);
  addWorld11WaterWave(position, pointX, pointZ, -0.7809f, -0.6247f,
      0.12f, 3.4f, 0.58f, 0.16f, 35.0f, -58.0f, 4.4f,
      simulationTime);
  return position;
}

}  // namespace detail

/// <summary>
/// CPU counterpart of world11WaterSurfaceAtWorldXZ in World11WaterWaves.hpp.
/// It inverts horizontal Gerstner displacement before returning the visible
/// surface height at a world-space XZ coordinate.
/// </summary>
inline float world11WaterSurfaceHeight(
    float worldX, float worldZ, float simulationTime) {
  float parameterX = worldX;
  float parameterZ = worldZ;
  for (int iteration = 0; iteration < 3; ++iteration) {
    const detail::World11WaterPoint displaced =
        detail::world11WaterDisplacedPosition(
            parameterX, parameterZ, simulationTime);
    parameterX += worldX - displaced.x;
    parameterZ += worldZ - displaced.z;
  }
  return detail::world11WaterDisplacedPosition(
      parameterX, parameterZ, simulationTime).y;
}

}  // namespace hg::world
