#pragma once

namespace hg {

/// <summary>
/// Static bridge for runtime lifecycle, world control, and camera state.
/// </summary>
class RuntimeBridge {
public:
  /// <summary>Configures dynamic world capacity in C++ runtime.</summary>
  static void configureRuntimeWorldCount(int world_count);
  /// <summary>Initializes runtime and graphics context.</summary>
  static void initRuntime();
  /// <summary>Begins frame and processes input/update state.</summary>
  static void beginFrame();
  /// <summary>Ends frame, presents buffers, and polls events.</summary>
  static void endFrame();
  /// <returns>True when runtime window should close.</returns>
  static bool shouldCloseRuntime();
  /// <summary>Shuts down runtime and windowing.</summary>
  static void shutdownRuntime();
  /// <returns>Current world id from runtime.</returns>
  static int currentWorld();
  /// <summary>Switches runtime to target world id.</summary>
  static void switchWorld(int world_id);
  /// <returns>Current camera X in local world coordinates.</returns>
  static double cameraX();
  /// <returns>Current camera Y in local world coordinates.</returns>
  static double cameraY();
  /// <returns>Current camera Z in local world coordinates.</returns>
  static double cameraZ();
  /// <returns>Current camera forward-vector X component.</returns>
  static double cameraFrontX();
  /// <returns>Current camera forward-vector Y component.</returns>
  static double cameraFrontY();
  /// <returns>Current camera forward-vector Z component.</returns>
  static double cameraFrontZ();
  /// <returns>Current camera up-vector X component.</returns>
  static double cameraUpX();
  /// <returns>Current camera up-vector Y component.</returns>
  static double cameraUpY();
  /// <returns>Current camera up-vector Z component.</returns>
  static double cameraUpZ();
  /// <summary>Overrides runtime camera position and syncs global coordinates.</summary>
  static void setCameraPosition(double x, double y, double z);
  /// <returns>True if Space key is currently pressed in runtime window.</returns>
  static bool isSpacePressed();
  /// <summary>Sets whether world uses C++ vertical jump/gravity physics.</summary>
  static void setCppVerticalMotionWorld(int world_id, bool enabled);
  /// <returns>True when world uses C++ vertical jump/gravity physics.</returns>
  static bool isCppVerticalMotionWorld(int world_id);
  /// <summary>Sets whether world uses gravity-based movement in C++ runtime.</summary>
  static void setWorldGravityPhysicsEnabled(int world_id, bool enabled);
  /// <returns>True when gravity-based movement is enabled for world.</returns>
  static bool isWorldGravityPhysicsEnabled(int world_id);
};

}  // namespace hg
