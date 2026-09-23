#pragma once

namespace hg {

/// <summary>Translates WASD/Space/C key state into camera movement.</summary>
class KeyboardMovementController {
public:
  /// <summary>
  /// Reads WASD (strafe/forward), Space/C (vertical, when the active world does not use
  /// physics-based vertical motion) and Left Shift (sprint) from
  /// <see cref="RuntimeInputSnapshot"/>, then moves the camera via
  /// <see cref="CameraMovementApplicator"/>.
  /// </summary>
  /// <param name="dt">Frame delta time in seconds, used to scale movement speed.</param>
  /// <remarks>Has side effects on global runtime camera state; safe to call every frame even if no key is pressed.</remarks>
  static void updateFromInputSnapshot(double dt);
};

}  // namespace hg
