#pragma once

namespace hg {

/// <summary>Owns process-wide state for the isolated OpenGL 3.3 Core path.</summary>
class Gl33RuntimeState {
public:
  /// <summary>Loads Core functions and configures depth and blending state.</summary>
  static void initialize();
  /// <summary>Clears the current Core framebuffer without using fixed-function matrices.</summary>
  static void beginFrame(int framebufferWidth, int framebufferHeight);
  /// <summary>Prints driver, context profile and launch-selection diagnostics.</summary>
  static void printDiagnostics(const char* selectedRenderer, const char* selectedWorld);
};

}  // namespace hg
