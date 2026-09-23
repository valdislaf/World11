#pragma once

namespace hg {

/// <summary>Global camera pose and mouse-look bookkeeping, exposed to the C bridge in <c>hg_runtime_api.h</c>.</summary>
struct RuntimeCameraState {
  double pos[3] = {1.5, 1.0, 11.5};
  double front[3] = {0.0, 0.0, -1.0};
  double up[3] = {0.0, 1.0, 0.0};
  double yaw = 0.0;
  double pitch = 0.0;
  /// <summary>True until the first mouse sample is received, so the initial cursor position does not produce a jump.</summary>
  bool first_mouse = true;
  double last_x = 400.0;
  double last_y = 300.0;
  /// <summary>Last camera speed reported to the HUD, in world units per second.</summary>
  double hud_speed = 0.0;
};

/// <returns>Reference to the single process-wide <see cref="RuntimeCameraState"/> instance (lazily constructed on first use).</returns>
RuntimeCameraState& runtimeCameraState();

}  // namespace hg
