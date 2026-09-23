#pragma once

#include <cstdint>
#include <memory>

namespace hg::render::gl33 {

/// <summary>Camera data consumed by the isolated Core renderer.</summary>
struct Gl33Camera {
  float position[3];
  float front[3];
  float up[3];
  int framebufferWidth;
  int framebufferHeight;
};

/// <summary>OpenGL 3.3 Core renderer for the first World 11 vertical slice.</summary>
class Gl33Renderer final {
public:
  Gl33Renderer();
  ~Gl33Renderer();
  Gl33Renderer(const Gl33Renderer&) = delete;
  Gl33Renderer& operator=(const Gl33Renderer&) = delete;

  /// <summary>Binds and clears the resize-aware color/depth target for World 11.</summary>
  void beginOpaquePass(const Gl33Camera& camera);
  /// <summary>Draws the procedural seabed into the active opaque target.</summary>
  void renderSeabed(const Gl33Camera& camera);
  /// <summary>Renders the displaced water surface into its own depth texture.</summary>
  void renderWaterSurfaceDepth(const Gl33Camera& camera, float time);
  /// <returns>The current framebuffer-sized water-surface depth texture.</returns>
  std::uint32_t waterSurfaceDepthTexture() const;
  /// <summary>
  /// Copies the opaque target to the default framebuffer, then composites water from
  /// screen-space color/depth using Beer-Lambert absorption and Fresnel reflection.
  /// </summary>
  void composeWater(const Gl33Camera& camera, float time);

private:
  struct Resources;
  void ensureInitialized();
  void ensureSceneTarget(int width, int height);

  std::unique_ptr<Resources> resources_;
};

}  // namespace hg::render::gl33
