#include "world/collision/CollisionWorld.hpp"

namespace hg::world::collision {

void CollisionWorld::addAabb(const std::string& id, const Vec3& min, const Vec3& max) {
  colliders_.push_back(Collider{id, Aabb{min, max}});
}

const std::vector<Collider>& CollisionWorld::colliders() const {
  return colliders_;
}

}  // namespace hg::world::collision
