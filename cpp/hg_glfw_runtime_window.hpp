#pragma once

struct GLFWwindow;

namespace hg {

/// <summary>Owns the single GLFW window and GL context used by the runtime.</summary>
class GlfwRuntimeWindow {
public:
  /// <summary>
  /// Initializes GLFW, creates the application window and makes its GL context current.
  /// Idempotent: does nothing if a window already exists. Terminates the process on failure.
  /// </summary>
  static void initialize();
  /// <returns>The owned window handle, or <c>nullptr</c> if <see cref="initialize"/> has not been called yet.</returns>
  static GLFWwindow* window();
  /// <returns>True if the window has been created and GLFW reported a close request (e.g. Escape key or OS close).</returns>
  static bool shouldClose();
  /// <summary>Destroys the window (if any) and terminates GLFW. Safe to call even if not initialized.</summary>
  static void shutdown();
};

}  // namespace hg
