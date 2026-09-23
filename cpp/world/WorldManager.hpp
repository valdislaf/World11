#pragma once

#include "hg_interfaces.hpp"
#include "world/WorldRegistry.hpp"

#include <memory>

namespace hg::world {

/// <summary>High-level world switch service built on top of WorldRegistry.</summary>
class WorldManager {
public:
  /// <summary>Constructs manager bound to shared registry.</summary>
  /// <param name="registry">World registry used to create instances and look up definitions; not owned, must outlive this object.</param>
  explicit WorldManager(const WorldRegistry& registry);

  /// <summary>Creates target world and switches runtime to it.</summary>
  /// <param name="world_id">Registered world id to activate.</param>
  /// <returns>Newly constructed world instance, owned by the caller.</returns>
  std::unique_ptr<IWorld> createAndSwitch(int world_id);
  /// <returns>Current world id last switched by manager.</returns>
  int currentWorldId() const;
  /// <returns>Definition of currently selected world.</returns>
  const WorldDefinition& currentDefinition() const;

private:
  const WorldRegistry& registry_;
  int currentWorldId_;
};

}  // namespace hg::world
