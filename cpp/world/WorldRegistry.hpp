#pragma once

#include "hg_interfaces.hpp"
#include "world/WorldDefinition.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hg::world {

using WorldFactory = std::function<std::unique_ptr<IWorld>()>;

/// <summary>Registry of world definitions and factories.</summary>
class WorldRegistry {
public:
  /// <summary>Registers or replaces the world entry keyed by <paramref name="definition"/>'s id.</summary>
  /// <param name="definition">Static metadata (id, name, assets) describing the world. <c>definition.id</c> must be greater than zero.</param>
  /// <param name="factory">Callable that constructs a new <see cref="IWorld"/> instance for this world id; invoked once per <see cref="createWorld"/> call. Must not be empty.</param>
  /// <remarks>Throws <c>std::invalid_argument</c> if <paramref name="factory"/> is empty or <c>definition.id</c> is not positive.</remarks>
  void registerWorld(WorldDefinition definition, WorldFactory factory);
  /// <returns>True when world id is present in registry.</returns>
  bool hasWorld(int world_id) const;
  /// <param name="world_id">Registered world id (see <see cref="hasWorld"/>).</param>
  /// <returns>Newly constructed world instance, owned by the caller.</returns>
  /// <remarks>Throws <c>std::out_of_range</c> if <paramref name="world_id"/> is not registered.</remarks>
  std::unique_ptr<IWorld> createWorld(int world_id) const;
  /// <param name="world_id">Registered world id (see <see cref="hasWorld"/>).</param>
  /// <returns>Reference to the stored definition, valid as long as the registry entry exists.</returns>
  /// <remarks>Throws <c>std::out_of_range</c> if <paramref name="world_id"/> is not registered.</remarks>
  const WorldDefinition& definition(int world_id) const;
  /// <returns>List of all registered world ids.</returns>
  std::vector<int> worldIds() const;
  /// <returns>Number of registered worlds.</returns>
  int worldCount() const;
  /// <returns>Largest registered world id, or 0 when registry is empty.</returns>
  int maxWorldId() const;

private:
  struct Entry {
    WorldDefinition definition;
    WorldFactory factory;
  };

  std::unordered_map<int, Entry> entries_;
};

}  // namespace hg::world
