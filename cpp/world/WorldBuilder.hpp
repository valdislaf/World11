#pragma once

#include "WorldDefinition.hpp"

#include <string>

namespace hg::world {

/// <summary>Fluent builder for design-time world metadata.</summary>
class WorldBuilder {
public:
  /// <summary>Starts a world definition.</summary>
  WorldBuilder(int id, std::string name);

  /// <summary>Configures gravity and jump physics for vertical camera movement in this world.</summary>
  /// <param name="enabled">True to enable gravity physics; false to keep free vertical movement.</param>
  /// <returns>This builder for fluent chaining.</returns>
  WorldBuilder& withGravityPhysics(bool enabled = true);

  /// <summary>Adds static object metadata.</summary>
  WorldBuilder& addStaticObject(const std::string& id,
                                const std::string& mesh,
                                double x,
                                double y,
                                double z);
  /// <summary>Adds light metadata.</summary>
  WorldBuilder& addLight(const std::string& id,
                         double r,
                         double g,
                         double b,
                         double intensity);
  /// <summary>Adds portal metadata.</summary>
  WorldBuilder& addPortal(const std::string& id,
                          int target_world_id,
                          double entry_x,
                          double entry_y,
                          double entry_z);

  /// <summary>Builds definition from lvalue builder.</summary>
  WorldDefinition build() &;
  /// <summary>Builds definition from rvalue builder.</summary>
  WorldDefinition build() &&;

private:
  WorldDefinition world_;
};

}  // namespace hg::world
