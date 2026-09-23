#pragma once

namespace hg {

/// <summary>Translates accumulated mouse cursor movement into camera yaw/pitch.</summary>
class MouseLookController {
public:
  /// <summary>
  /// Reads the current cursor position from <see cref="RuntimeInputSnapshot"/>, computes the
  /// delta since the last call, and updates camera yaw/pitch (clamped to +/-89 degrees pitch).
  /// On the very first call after enabling the camera, only stores the initial position.
  /// </summary>
  /// <remarks>Has side effects on global runtime camera state; intended to be called once per frame.</remarks>
  static void updateFromInputSnapshot();
};

}  // namespace hg
