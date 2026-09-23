#include "world/WorldManager.hpp"

#include "hg_runtime_bridge.hpp"

namespace hg::world {

WorldManager::WorldManager(const WorldRegistry& registry)
    : registry_(registry), currentWorldId_(0) {}

std::unique_ptr<IWorld> WorldManager::createAndSwitch(int world_id) {
  auto world = registry_.createWorld(world_id);
  RuntimeBridge::switchWorld(world_id);
  currentWorldId_ = world_id;
  return world;
}

int WorldManager::currentWorldId() const {
  return currentWorldId_;
}

const WorldDefinition& WorldManager::currentDefinition() const {
  return registry_.definition(currentWorldId_);
}

}  // namespace hg::world
