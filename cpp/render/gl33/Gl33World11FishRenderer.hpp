#pragma once

#include "render/gl33/Gl33Renderer.hpp"
#include "world/World11FishTrajectory.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace hg::render::gl33 {

class Gl33ShaderProgram;

/// <summary>
/// Draws a small deterministic fish population sharing one articulated mesh
/// and texture, with individual trajectories, scales and color tints.
/// </summary>
class Gl33World11FishRenderer final {
public:
  explicit Gl33World11FishRenderer(
      std::uint64_t trajectorySeed = 0x574F524C44313146ULL);
  ~Gl33World11FishRenderer();
  Gl33World11FishRenderer(const Gl33World11FishRenderer&) = delete;
  Gl33World11FishRenderer& operator=(
      const Gl33World11FishRenderer&) = delete;

  /// <summary>Draws the fish, receiving shadows when a map is supplied.</summary>
  void render(const Gl33Camera& camera, float simulationTime,
              const Gl33ShadowMap* shadowMap = nullptr);
  /// <summary>Draws the fish silhouettes into the active shadow depth pass.</summary>
  void renderShadowDepth(float simulationTime, const Gl33ShadowMap& shadowMap);

private:
  struct Resources;
  void ensureInitialized();
  void drawPopulation(const Gl33ShaderProgram& program,
                      float simulationTime);

  std::vector<world::World11FishTrajectory> trajectories_;
  std::unique_ptr<Resources> resources_;
};

}  // namespace hg::render::gl33
