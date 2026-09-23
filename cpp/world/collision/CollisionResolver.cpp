#include "world/collision/CollisionResolver.hpp"

#include <algorithm>

namespace hg::world::collision {

Vec3 CollisionResolver::resolveMovement(const Vec3& current,
                                        const Vec3& desired,
                                        double radius,
                                        const CollisionWorld& world) const {
  Vec3 resolved = current;
  const Vec3 delta = {desired[0] - current[0], desired[1] - current[1], desired[2] - current[2]};

  for (int axis = 0; axis < 3; ++axis) {
    Vec3 candidate = resolved;
    candidate[axis] += delta[axis];

    bool blocked = false;
    for (const Collider& collider : world.colliders()) {
      if (sphereCollidesAabb(candidate, radius, collider.bounds)) {
        blocked = true;
        break;
      }
    }

    if (!blocked) {
      resolved = candidate;
    }
  }

  return resolved;
}

bool CollisionResolver::sphereCollidesAabb(const Vec3& center, double radius, const Aabb& aabb) {
  double closest[3] = {
      std::clamp(center[0], aabb.min[0], aabb.max[0]),
      std::clamp(center[1], aabb.min[1], aabb.max[1]),
      std::clamp(center[2], aabb.min[2], aabb.max[2]),
  };

  const double dx = center[0] - closest[0];
  const double dy = center[1] - closest[1];
  const double dz = center[2] - closest[2];
  return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

}  // namespace hg::world::collision
