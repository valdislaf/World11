#include "world/WorldHost.hpp"

#include "hg_runtime_bridge.hpp"

#include <stdexcept>

namespace hg::world {

WorldHost::WorldHost(WorldManager& manager, int initial_world_id) : manager_(manager) {
  activeWorld_ = manager_.createAndSwitch(initial_world_id);
  if (!activeWorld_) {
    throw std::runtime_error("WorldHost failed to create initial world");
  }
}

void WorldHost::tick(Engine& engine, double dt) {
  activeWorld_->tick(engine, dt);

  const int runtime_world_id = RuntimeBridge::currentWorld();
  if (runtime_world_id != manager_.currentWorldId()) {
    activeWorld_ = manager_.createAndSwitch(runtime_world_id);
  }
}

}  // namespace hg::world
