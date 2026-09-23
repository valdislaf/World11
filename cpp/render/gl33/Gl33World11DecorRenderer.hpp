#pragma once

#include "render/gl33/Gl33Renderer.hpp"
#include "world/World11DecorGenerator.hpp"

#include <cstdint>
#include <memory>

namespace hg::render::gl33 {

enum class Gl33BubblePass {
  BehindWater = 0,
  InFrontOfWater = 1,
};

/// <summary>
/// Camera-centered, cached and instanced renderer for procedural World 11
/// seaweed clusters and coral colonies.
/// </summary>
class Gl33World11DecorRenderer final {
public:
  explicit Gl33World11DecorRenderer(world::World11DecorSeeds seeds = {});
  ~Gl33World11DecorRenderer();
  Gl33World11DecorRenderer(const Gl33World11DecorRenderer&) = delete;
  Gl33World11DecorRenderer& operator=(const Gl33World11DecorRenderer&) = delete;

  void setSeeds(world::World11DecorSeeds seeds);
  void render(const Gl33Camera& camera, float time);
  void renderBubbles(const Gl33Camera& camera, float time,
                     Gl33BubblePass pass,
                     std::uint32_t waterSurfaceDepthTexture);

private:
  struct Resources;
  struct Cache;

  void ensureInitialized();
  void updateVisibleChunks(const Gl33Camera& camera);
  void updateCoralInstances(const Gl33Camera& camera);

  world::World11DecorGenerator generator_;
  std::unique_ptr<Resources> resources_;
  std::unique_ptr<Cache> cache_;
};

}  // namespace hg::render::gl33
