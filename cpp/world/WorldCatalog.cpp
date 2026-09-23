#include "world/WorldRegistry.hpp"
#include "world/CppWorldGl33.hpp"
#include "world/WorldBuilder.hpp"

namespace hg::world {

void registerBuiltInWorlds(WorldRegistry& registry) {
  const auto registerCoreWorld = [&registry](int id, const char* name, bool gravity) {
    WorldBuilder builder(id, name);
    if (gravity) {
      builder.withGravityPhysics();
    }
    registry.registerWorld(builder.build(), [id] { return std::make_unique<CppWorldGl33>(id); });
  };
  registerCoreWorld(6, "World 6 (OpenGL 3.3 Core)", false);
  registerCoreWorld(11, "World 11 (OpenGL 3.3 Core)", false);
}

}  // namespace hg::world
