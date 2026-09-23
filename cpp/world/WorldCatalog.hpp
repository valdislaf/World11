#pragma once

#include "world/WorldRegistry.hpp"

namespace hg::world {

/// <summary>Registers all built-in C++ worlds (definitions and factories) into a registry.</summary>
/// <param name="registry">Registry to populate; not owned, must outlive this call.</param>
void registerBuiltInWorlds(WorldRegistry& registry);

}  // namespace hg::world
