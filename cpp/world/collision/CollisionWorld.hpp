#pragma once

#include "world/collision/CollisionTypes.hpp"

#include <string>
#include <vector>

namespace hg::world::collision {

/// <summary>Named collider entry stored in collision world.</summary>
struct Collider {
  std::string id;
  Aabb bounds;
};

/// <summary>Container for world colliders used by resolver.</summary>
class CollisionWorld {
public:
  /// <summary>Adds an AABB collider with logical id.</summary>
  /// <param name="id">Logical identifier for the collider (for debugging/lookup); need not be unique.</param>
  /// <param name="min">Minimum corner of the box, in world coordinates.</param>
  /// <param name="max">Maximum corner of the box, in world coordinates.</param>
  void addAabb(const std::string& id, const Vec3& min, const Vec3& max);
  /// <returns>Readonly collider list.</returns>
  const std::vector<Collider>& colliders() const;

private:
  std::vector<Collider> colliders_;
};

}  // namespace hg::world::collision
