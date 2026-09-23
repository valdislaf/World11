#pragma once

namespace hg {

/// <summary>Translates the first connected gamepad's state into camera look and movement.</summary>
class GamepadController {
public:
  /// <summary>
  /// Reads gamepad 1 from <see cref="RuntimeInputSnapshot"/> (if connected), applies deadzone
  /// filtering to sticks/triggers, updates camera yaw/pitch, and moves the camera via
  /// <see cref="CameraMovementApplicator"/>. Also requests application close on Start/Back.
  /// </summary>
  /// <param name="dt">Frame delta time in seconds, used to scale look and movement speed.</param>
  /// <remarks>No-op if gamepad 1 is not connected. Has side effects on global runtime camera/window state.</remarks>
  static void updateFromInputSnapshot(double dt);
};

}  // namespace hg
