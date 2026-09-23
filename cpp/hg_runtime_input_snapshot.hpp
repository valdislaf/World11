#pragma once

#include <cstdint>

namespace hg {

/// <summary>Snapshot of gamepad 1 button/axis state, as reported by GLFW's gamepad mapping.</summary>
struct RuntimeGamepadState {
  std::int8_t buttons[15]{};
  float axes[6]{};
};

/// <summary>
/// Caches tracked keyboard, cursor and gamepad state once per frame so multiple controllers
/// can query input without repeatedly hitting the GLFW API.
/// </summary>
class RuntimeInputSnapshot {
public:
  /// <summary>Polls GLFW for the tracked keys, cursor position and gamepad 1 state, replacing the cached snapshot.</summary>
  /// <remarks>Intended to be called once per frame. If no window exists yet, resets to default/neutral state.</remarks>
  static void poll();
  /// <summary>Implicitly calls <see cref="poll"/> on the first query of a frame if not already done.</summary>
  /// <param name="key">A <c>GLFW_KEY_*</c> code from the tracked key set.</param>
  /// <returns><c>GLFW_PRESS</c> or <c>GLFW_RELEASE</c> for the given key; <c>GLFW_RELEASE</c> if the key is not tracked.</returns>
  static int keyState(int key);
  /// <summary>Reads the cursor position captured in the current snapshot.</summary>
  /// <param name="xpos">Receives the cursor X position in screen coordinates.</param>
  /// <param name="ypos">Receives the cursor Y position in screen coordinates.</param>
  static void cursorPosition(double& xpos, double& ypos);
  /// <param name="jid">Joystick id, e.g. <c>GLFW_JOYSTICK_1</c>.</param>
  /// <returns><c>GLFW_TRUE</c> if the given joystick is connected and recognized as a gamepad; only <c>GLFW_JOYSTICK_1</c> is supported.</returns>
  static int isGamepad(int jid);
  /// <param name="jid">Joystick id, e.g. <c>GLFW_JOYSTICK_1</c>.</param>
  /// <param name="state">Receives the cached gamepad state; reset to defaults if unavailable.</param>
  /// <returns><c>GLFW_TRUE</c> if state was available and written, otherwise <c>GLFW_FALSE</c>.</returns>
  static int gamepadState(int jid, RuntimeGamepadState& state);
  /// <summary>Requests that the runtime window close on the next event poll (e.g. in response to a quit input).</summary>
  static void requestClose();
};

}  // namespace hg
