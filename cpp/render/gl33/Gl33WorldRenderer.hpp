#pragma once

#include "render/gl33/Gl33Renderer.hpp"
#include "world/World11DecorGenerator.hpp"

#include <cstddef>
#include <memory>

namespace hg::world {
struct Portal;
}

namespace hg::render::gl33 {

class Gl33Skybox;
class Gl33World11DecorRenderer;
class Gl33World11FishRenderer;

/// <summary>Core-renderer frame data shared by modern implementations of worlds 6–12.</summary>
struct Gl33WorldFrame {
  int worldId;
  Gl33Camera camera;
  float time;
  float doorOpenAmount;
  const world::Portal* portals;
  std::size_t portalCount;
};

/// <summary>Renders recognizable Core-profile versions of all gameplay worlds.</summary>
class Gl33WorldRenderer final {
public:
  explicit Gl33WorldRenderer(int worldId);
  ~Gl33WorldRenderer();
  Gl33WorldRenderer(const Gl33WorldRenderer&) = delete;
  Gl33WorldRenderer& operator=(const Gl33WorldRenderer&) = delete;

  void render(const Gl33WorldFrame& frame);
  void setWorld11DecorSeeds(world::World11DecorSeeds seeds);

private:
  struct Resources;
  void ensureInitialized();

  int worldId_;
  std::unique_ptr<Resources> resources_;
  std::unique_ptr<Gl33Renderer> oceanRenderer_;
  std::unique_ptr<Gl33World11DecorRenderer> world11DecorRenderer_;
  std::unique_ptr<Gl33World11FishRenderer> world11FishRenderer_;
  std::unique_ptr<Gl33ShadowMap> world11ShadowMap_;
  std::unique_ptr<Gl33Skybox> skybox_;
};

}  // namespace hg::render::gl33
