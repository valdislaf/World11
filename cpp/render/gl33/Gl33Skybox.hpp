#pragma once

#include "render/gl33/Gl33Renderer.hpp"

#include <array>
#include <memory>
#include <string>

namespace hg::render::gl33 {

/// <summary>GLSL 330 atlas skybox backed by an encrypted FGET texture.</summary>
class Gl33Skybox final {
public:
  Gl33Skybox(std::string fgetPath, float radius,
             float sourceSideHorizon = 0.5f,
             float targetSideHorizon = 0.5f,
             bool underwaterMask = false,
             float waterSurfaceY = 0.0f,
             std::array<float, 3> underwaterFogColor = {0.0f, 0.0f, 0.0f});
  ~Gl33Skybox();
  Gl33Skybox(const Gl33Skybox&) = delete;
  Gl33Skybox& operator=(const Gl33Skybox&) = delete;

  /// <summary>Draws an infinite background; centerY only controls the painted horizon.</summary>
  void render(const Gl33Camera& camera, float centerY);

private:
  struct Resources;
  void ensureInitialized();

  std::string fgetPath_;
  float radius_;
  float sourceSideHorizon_;
  float targetSideHorizon_;
  bool underwaterMask_;
  float waterSurfaceY_;
  std::array<float, 3> underwaterFogColor_;
  std::unique_ptr<Resources> resources_;
};

}  // namespace hg::render::gl33
