#pragma once

namespace hg {

/// <summary>Updates the GLFW window title with a periodically recomputed FPS counter.</summary>
class FrameTitleController {
public:
  /// <summary>Restarts the FPS accumulator; call once when the runtime window is (re)created.</summary>
  static void reset();
  /// <summary>
  /// Counts the current frame and, once at least a second has elapsed since the last
  /// update, rewrites the window title with the averaged FPS value.
  /// </summary>
  /// <remarks>No-op if no GLFW window is currently active. Has the side effect of calling <c>glfwSetWindowTitle</c>.</remarks>
  static void update();
};

}  // namespace hg
