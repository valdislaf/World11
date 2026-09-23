#pragma once

#include "hg_interfaces.hpp"
#include "world/WorldManager.hpp"

#include <memory>

namespace hg::world {

/// <summary>Runtime router that owns active world instance and switches by runtime world id.</summary>
class WorldHost final : public IWorld {
public:
  /// <summary>Creates host and activates initial world.</summary>
  /// <param name="manager">World manager used to create world instances; not owned, must outlive this object.</param>
  /// <param name="initial_world_id">Registered world id to activate immediately.</param>
  WorldHost(WorldManager& manager, int initial_world_id);
  /// <summary>Ticks active world and hot-switches instance when runtime world id changes.</summary>
  /// <param name="engine">Engine facade with frame/time state.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  void tick(Engine& engine, double dt) override;

private:
  WorldManager& manager_;
  std::unique_ptr<IWorld> activeWorld_;
};

}  // namespace hg::world
