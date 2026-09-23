#include "hg_runtime_input_snapshot.hpp"

#include "hg_glfw_runtime_window.hpp"

#include <GLFW/glfw3.h>

#include <array>

namespace hg {

namespace {

constexpr int kButtonCount = 15;
constexpr int kAxisCount = 6;
constexpr std::array<int, 8> kTrackedKeys = {
    GLFW_KEY_W,
    GLFW_KEY_A,
    GLFW_KEY_S,
    GLFW_KEY_D,
    GLFW_KEY_C,
    GLFW_KEY_SPACE,
    GLFW_KEY_LEFT_SHIFT,
    GLFW_KEY_F1,
};

std::array<int, GLFW_KEY_LAST + 1> g_keys{};
double g_cursor_x = 400.0;
double g_cursor_y = 300.0;
bool g_gamepad_available = false;
RuntimeGamepadState g_gamepad_state{};
bool g_has_snapshot = false;

void clearGamepad() {
  g_gamepad_available = false;
  g_gamepad_state = RuntimeGamepadState{};
}

void ensureSnapshot() {
  if (!g_has_snapshot) {
    RuntimeInputSnapshot::poll();
  }
}

}  // namespace

void RuntimeInputSnapshot::poll() {
  g_keys.fill(GLFW_RELEASE);
  g_has_snapshot = true;

  auto* window = GlfwRuntimeWindow::window();
  if (window == nullptr) {
    g_cursor_x = 400.0;
    g_cursor_y = 300.0;
    clearGamepad();
    return;
  }

  for (const int key : kTrackedKeys) {
    if (key >= 0 && key < static_cast<int>(g_keys.size())) {
      g_keys[static_cast<std::size_t>(key)] = glfwGetKey(window, key);
    }
  }

  glfwGetCursorPos(window, &g_cursor_x, &g_cursor_y);

  clearGamepad();
  if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) == GLFW_FALSE) {
    return;
  }

  GLFWgamepadstate state{};
  if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state) == GLFW_FALSE) {
    return;
  }

  g_gamepad_available = true;
  for (int i = 0; i < kButtonCount; ++i) {
    g_gamepad_state.buttons[i] = static_cast<std::int8_t>(state.buttons[i]);
  }
  for (int i = 0; i < kAxisCount; ++i) {
    g_gamepad_state.axes[i] = state.axes[i];
  }
}

int RuntimeInputSnapshot::keyState(int key) {
  ensureSnapshot();
  if (key < 0 || key >= static_cast<int>(g_keys.size())) {
    return GLFW_RELEASE;
  }
  return g_keys[static_cast<std::size_t>(key)];
}

void RuntimeInputSnapshot::cursorPosition(double& xpos, double& ypos) {
  ensureSnapshot();
  xpos = g_cursor_x;
  ypos = g_cursor_y;
}

int RuntimeInputSnapshot::isGamepad(int jid) {
  ensureSnapshot();
  if (jid != GLFW_JOYSTICK_1) {
    return GLFW_FALSE;
  }
  return g_gamepad_available ? GLFW_TRUE : GLFW_FALSE;
}

int RuntimeInputSnapshot::gamepadState(int jid, RuntimeGamepadState& state) {
  ensureSnapshot();
  if (jid != GLFW_JOYSTICK_1 || !g_gamepad_available) {
    state = RuntimeGamepadState{};
    return GLFW_FALSE;
  }
  state = g_gamepad_state;
  return GLFW_TRUE;
}

void RuntimeInputSnapshot::requestClose() {
  auto* window = GlfwRuntimeWindow::window();
  if (window != nullptr) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

}  // namespace hg
