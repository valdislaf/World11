#pragma once

#include "render/gl33/Gl33WorldRenderer.hpp"
#include "world/BaseWorld.hpp"

namespace hg::world {

/// <summary>Core-profile implementation used for runtime worlds 6–12.</summary>
class CppWorldGl33 final : public BaseWorld {
public:
  /// <summary>Creates the requested modern world while preserving its existing runtime id.</summary>
  explicit CppWorldGl33(int worldId);

  int getWorldId() const override;

private:
  void initColliders() override;
  void beforeBeginFrame(Engine& engine, double dt) override;
  double groundContactAt(double x, double z) const override;
  double cameraRadius() const override;
  void updateDoor(const collision::Vec3& cameraPosition, double dt) override;
  collision::Vec3 applyDoorBlock(const collision::Vec3& cameraPosition,
                                 const collision::Vec3& resolved) const override;
  void drawScene() const override;
  void drawPortals() const override;

  int worldId_;
  mutable render::gl33::Gl33WorldRenderer renderer_;
  float sceneTime_ = 0.0f;
};

}  // namespace hg::world
