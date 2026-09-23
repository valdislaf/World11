#include "world/WorldBuilder.hpp"

#include <utility>

namespace hg::world {

WorldBuilder::WorldBuilder(int id, std::string name) {
  world_.id = id;
  world_.name = std::move(name);
}

WorldBuilder& WorldBuilder::withGravityPhysics(bool enabled) {
  world_.gravity_physics_enabled = enabled;
  return *this;
}

WorldBuilder& WorldBuilder::addStaticObject(const std::string& id,
                                            const std::string& mesh,
                                            double x,
                                            double y,
                                            double z) {
  StaticObjectDefinition object;
  object.id = id;
  object.mesh = mesh;
  object.position[0] = x;
  object.position[1] = y;
  object.position[2] = z;
  world_.objects.push_back(std::move(object));
  return *this;
}

WorldBuilder& WorldBuilder::addLight(const std::string& id,
                                     double r,
                                     double g,
                                     double b,
                                     double intensity) {
  LightDefinition light;
  light.id = id;
  light.color[0] = r;
  light.color[1] = g;
  light.color[2] = b;
  light.intensity = intensity;
  world_.lights.push_back(std::move(light));
  return *this;
}

WorldBuilder& WorldBuilder::addPortal(const std::string& id,
                                      int target_world_id,
                                      double entry_x,
                                      double entry_y,
                                      double entry_z) {
  PortalDefinition portal;
  portal.id = id;
  portal.target_world_id = target_world_id;
  portal.target_entry[0] = entry_x;
  portal.target_entry[1] = entry_y;
  portal.target_entry[2] = entry_z;
  world_.portals.push_back(std::move(portal));
  return *this;
}

WorldDefinition WorldBuilder::build() & {
  return std::move(world_);
}

WorldDefinition WorldBuilder::build() && {
  return std::move(world_);
}

}  // namespace hg::world
