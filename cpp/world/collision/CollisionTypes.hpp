#pragma once

#include <array>

namespace hg::world::collision {

/// <summary>Simple 3D vector type used by collision APIs.</summary>
using Vec3 = std::array<double, 3>;

/// <summary>Axis-aligned bounding box.</summary>
struct Aabb {
  Vec3 min;
  Vec3 max;
};

}  // namespace hg::world::collision
