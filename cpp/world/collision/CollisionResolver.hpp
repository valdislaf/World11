#pragma once

#include "world/collision/CollisionTypes.hpp"
#include "world/collision/CollisionWorld.hpp"

namespace hg::world::collision {

/// <summary>Movement resolver for sphere-vs-AABB collisions.</summary>
class CollisionResolver {
public:
  /// <summary>
  /// Resolves movement from current to desired position with axis-wise clipping.
  /// </summary>
  /// <param name="current">Position before movement.</param>
  /// <param name="desired">Position after movement intent.</param>
  /// <param name="radius">Sphere collision radius.</param>
  /// <param name="world">Collider storage for active world.</param>
  /// <returns>Resolved non-penetrating position.</returns>
  Vec3 resolveMovement(const Vec3& current,
                       const Vec3& desired,
                       double radius,
                       const CollisionWorld& world) const;

private:
  static bool sphereCollidesAabb(const Vec3& center, double radius, const Aabb& aabb);
};

}  // namespace hg::world::collision
