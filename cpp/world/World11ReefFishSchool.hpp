#pragma once

#include "world/World11FishTrajectory.hpp"
#include "world/World11Landmarks.hpp"
#include "world/World11Seabed.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace hg::world {

/// <summary>Number of reef butterflyfish circling the coral colony.</summary>
inline constexpr std::size_t kWorld11ReefFishCount = 5;
/// <summary>Model half height of a reef fish before its per-fish scale.</summary>
inline constexpr float kWorld11ReefFishHalfHeight = 0.20f;

/// <summary>
/// Deterministic trajectory of reef fish <paramref name="index"/>. The school
/// keeps to the colony and at most 3.2 m above the seabed.
/// </summary>
inline World11FishTrajectory world11ReefFishTrajectory(std::size_t index) {
  const World11Landmark colony = kWorld11Colony;
  World11FishMovementVolume volume;
  volume.center = {colony.x, world11SeabedHeight(colony.x, colony.z) + 2.0f,
                   colony.z};
  volume.horizontalRadius = 9.0f;
  volume.surfaceClearance = 0.5f;
  volume.seabedClearance = 0.35f;
  volume.fishHalfHeight = kWorld11ReefFishHalfHeight;
  volume.maximumSeabedHeight = 3.2f;
  const float angle = 1.2566371f * static_cast<float>(index);
  const World11FishVec3 start{
      colony.x + 5.0f * std::cos(angle),
      volume.center.y + 0.3f * static_cast<float>(index % 3),
      colony.z + 5.0f * std::sin(angle)};
  return World11FishTrajectory(
      0x5245454646495348ULL ^
          (0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(index + 1)),
      start, volume);
}

}  // namespace hg::world
