#pragma once

#include "render/gl33/Gl33WorldRenderer.hpp"
#include "world/BaseWorld.hpp"

namespace hg::world {

/// <summary>
/// Copy-ready BaseWorld template for a minimal pure C++ world.
/// </summary>
/// <remarks>
/// Rename the class/file to CppWorldN and replace the invalid template id in the .cpp file
/// with a unique registered world id before using the copied world.
/// </remarks>
class CppWorldTemplate final : public BaseWorld {
public:
  /// <summary>Creates the world template and registers base colliders.</summary>
  CppWorldTemplate();

  /// <returns>The placeholder template id; replace it with the copied world's unique id.</returns>
  int getWorldId() const override;

private:
  void initColliders() override;
  void beforeBeginFrame(Engine& engine, double dt) override;
  void drawScene() const override;

  mutable render::gl33::Gl33WorldRenderer renderer_;
  float sceneTime_ = 0.0f;
};

}  // namespace hg::world
