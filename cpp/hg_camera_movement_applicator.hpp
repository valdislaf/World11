#pragma once

namespace hg {

/// <summary>Per-frame camera position offset expressed in world units.</summary>
struct CameraMovementDelta {
  double x;
  double y;
  double z;
};

/// <summary>Applies computed camera movement to the runtime camera state.</summary>
class CameraMovementApplicator {
public:
  /// <summary>
  /// Adds <paramref name="displacement"/> to the current camera position when moving,
  /// updates the reported camera speed, and re-syncs the active world with the world
  /// whose anchor is nearest to the resulting global camera position.
  /// </summary>
  /// <param name="displacement">Position delta to add for this frame.</param>
  /// <param name="moving">True if the camera actually moved this frame; when false only the speed is reset to zero.</param>
  /// <param name="speed_now">Current camera speed to report to the runtime, in world units per second.</param>
  /// <remarks>Has side effects: mutates global runtime camera state via <c>RuntimeBridge</c> and <c>hg_runtime_*</c> calls.</remarks>
  static void apply(const CameraMovementDelta& displacement, bool moving, double speed_now);
};

}  // namespace hg
