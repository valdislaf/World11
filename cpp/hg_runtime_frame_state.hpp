#pragma once

namespace hg {

/// <summary>Holds the current frame's delta time and framebuffer size for use by worlds and renderers.</summary>
class RuntimeFrameState {
public:
  /// <summary>Stores the values reported for the current frame. Intended to be called once per frame before rendering.</summary>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <param name="framebuffer_width">Current framebuffer width in pixels.</param>
  /// <param name="framebuffer_height">Current framebuffer height in pixels.</param>
  static void setFrame(double dt, int framebuffer_width, int framebuffer_height);
  /// <returns>Framebuffer width in pixels, as last set via <see cref="setFrame"/>.</returns>
  static int framebufferWidth();
  /// <returns>Framebuffer height in pixels, as last set via <see cref="setFrame"/>.</returns>
  static int framebufferHeight();
};

}  // namespace hg
