#pragma once

#include <string>
#include <vector>

namespace hg::world {

/// <summary>Design-time static object metadata for a world.</summary>
struct StaticObjectDefinition {
  std::string id;
  std::string mesh;
  double position[3] = {0.0, 0.0, 0.0};
};

/// <summary>Design-time light metadata for a world.</summary>
struct LightDefinition {
  std::string id;
  double color[3] = {1.0, 1.0, 1.0};
  double intensity = 1.0;
};

/// <summary>Design-time portal metadata for a world.</summary>
struct PortalDefinition {
  std::string id;
  int target_world_id = 1;
  double target_entry[3] = {0.0, 0.0, 0.0};
};

/// <summary>Aggregated world metadata used by C++ registry and tools.</summary>
struct WorldDefinition {
  int id = 0;
  std::string name;
  /// <summary>True when this world uses gravity and jump physics for vertical camera movement.</summary>
  bool gravity_physics_enabled = false;
  // Design-time metadata for C++ tools/editor.
  std::vector<StaticObjectDefinition> objects;
  std::vector<LightDefinition> lights;
  std::vector<PortalDefinition> portals;
};

}  // namespace hg::world
