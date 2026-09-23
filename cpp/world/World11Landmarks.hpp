#pragma once
#include "world/World11Seabed.hpp"

namespace hg::world {

/// <summary>Authored points of interest, independent of procedural decor seeds.</summary>
struct World11Landmark {
  float x;
  float z;
};
inline constexpr World11Landmark kWorld11Arch{-16.0f, -64.0f};
inline constexpr World11Landmark kWorld11Seep{-35.0f, -62.0f};
inline constexpr World11Landmark kWorld11Colony{34.0f, -82.0f};
inline const float kWorld11ReturnPortalY = world11SeabedHeight(0.0f, -20.0f) + 2.1f;

}  // namespace hg::world
