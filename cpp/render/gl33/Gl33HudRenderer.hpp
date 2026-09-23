#pragma once

namespace hg::render::gl33 {

/// <summary>Process-wide OpenGL 3.3 Core HUD pass.</summary>
class Gl33HudRenderer final {
public:
  /// <summary>Handles F1 and renders the HUD after the 3D scene.</summary>
  static void render();
  /// <summary>Releases GL resources while the context is still current.</summary>
  static void reset();
};

}  // namespace hg::render::gl33
