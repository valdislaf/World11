#include "hg_gamepad_controller.hpp"

#include "hg_camera_movement_applicator.hpp"
#include "hg_camera_tuning.hpp"
#include "hg_runtime_api.h"
#include "hg_runtime_input_snapshot.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace hg {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kGamepadDeadzone = 0.20;
constexpr double kGamepadLookSensitivity = 140.0;

struct Vec3 {
  double x;
  double y;
  double z;
};

Vec3 operator+(const Vec3& a, const Vec3& b) {
  return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
  return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(double scalar, const Vec3& v) {
  return Vec3{scalar * v.x, scalar * v.y, scalar * v.z};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return Vec3{
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

double dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(const Vec3& v) {
  const double len = std::sqrt(dot(v, v));
  if (len <= 1.0e-12) {
    return Vec3{0.0, 0.0, 0.0};
  }
  return Vec3{v.x / len, v.y / len, v.z / len};
}

double axisDeadzone(double value) {
  if (std::abs(value) < kGamepadDeadzone) {
    return 0.0;
  }
  return ((std::abs(value) - kGamepadDeadzone) / (1.0 - kGamepadDeadzone)) * (value < 0.0 ? -1.0 : 1.0);
}

void updateCameraVectors(double yaw, double pitch) {
  const double yaw_rad = yaw * kPi / 180.0;
  const double pitch_rad = pitch * kPi / 180.0;

  const double cy = std::cos(yaw_rad);
  const double sy = std::sin(yaw_rad);
  const double cp = std::cos(pitch_rad);
  const double sp = std::sin(pitch_rad);

  Vec3 front{
      sy * cp,
      sp,
      -cy * cp,
  };
  front = normalize(front);

  const Vec3 world_up{0.0, 1.0, 0.0};
  const Vec3 right = normalize(cross(front, world_up));
  const Vec3 up = normalize(cross(right, front));

  hg_runtime_set_camera_orientation(yaw, pitch, front.x, front.y, front.z, up.x, up.y, up.z);
}

bool pressed(const RuntimeGamepadState& state, int button) {
  return state.buttons[button] == GLFW_PRESS;
}

}  // namespace

void GamepadController::updateFromInputSnapshot(double dt) {
  if (RuntimeInputSnapshot::isGamepad(GLFW_JOYSTICK_1) == GLFW_FALSE) {
    return;
  }

  RuntimeGamepadState pad_state{};
  if (RuntimeInputSnapshot::gamepadState(GLFW_JOYSTICK_1, pad_state) == GLFW_FALSE) {
    return;
  }

  const double lx = axisDeadzone(static_cast<double>(pad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]));
  const double ly = axisDeadzone(static_cast<double>(pad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]));
  const double rx = axisDeadzone(static_cast<double>(pad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]));
  const double ry = axisDeadzone(static_cast<double>(pad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]));

  const double yaw = hg_runtime_camera_yaw() + rx * kGamepadLookSensitivity * dt;
  const double pitch = std::clamp(hg_runtime_camera_pitch() - ry * kGamepadLookSensitivity * dt, -89.0, 89.0);
  updateCameraVectors(yaw, pitch);

  double speed_now = camera_tuning::kMoveSpeedBase;
  if (pad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f) {
    speed_now = camera_tuning::kMoveSpeedBase * camera_tuning::kGamepadBoostFactor;
  }
  const double velocity = speed_now * dt;

  const Vec3 front{
      hg_runtime_camera_front_x(),
      hg_runtime_camera_front_y(),
      hg_runtime_camera_front_z(),
  };
  const Vec3 up{
      hg_runtime_camera_up_x(),
      hg_runtime_camera_up_y(),
      hg_runtime_camera_up_z(),
  };
  const Vec3 right = normalize(cross(front, up));

  Vec3 displacement{0.0, 0.0, 0.0};
  bool moving = false;

  if (std::abs(ly) > 0.0) {
    displacement = displacement - ly * velocity * front;
    moving = true;
  }
  if (std::abs(lx) > 0.0) {
    displacement = displacement + lx * velocity * right;
    moving = true;
  }

  const int world_id = hg_runtime_world();
  if (hg_runtime_is_cpp_vertical_motion_enabled(world_id) == 0) {
    if (pressed(pad_state, GLFW_GAMEPAD_BUTTON_A)) {
      displacement = displacement + velocity * up;
      moving = true;
    }
    if (pressed(pad_state, GLFW_GAMEPAD_BUTTON_B)) {
      displacement = displacement - velocity * up;
      moving = true;
    }
  }

  if (pressed(pad_state, GLFW_GAMEPAD_BUTTON_START) || pressed(pad_state, GLFW_GAMEPAD_BUTTON_BACK)) {
    RuntimeInputSnapshot::requestClose();
  }

  CameraMovementApplicator::apply(CameraMovementDelta{displacement.x, displacement.y, displacement.z},
                                  moving,
                                  speed_now);
}

}  // namespace hg
