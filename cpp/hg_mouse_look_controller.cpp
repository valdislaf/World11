#include "hg_mouse_look_controller.hpp"

#include "hg_runtime_api.h"
#include "hg_runtime_input_snapshot.hpp"

#include <algorithm>
#include <cmath>

namespace hg {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMouseSensitivity = 0.1;

struct Vec3 {
  double x;
  double y;
  double z;
};

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

}  // namespace

void MouseLookController::updateFromInputSnapshot() {
  double xpos = 400.0;
  double ypos = 300.0;
  RuntimeInputSnapshot::cursorPosition(xpos, ypos);

  if (hg_runtime_camera_first_mouse() != 0) {
    hg_runtime_set_camera_mouse_state(0, xpos, ypos);
    return;
  }

  const double last_x = hg_runtime_camera_last_mouse_x();
  const double last_y = hg_runtime_camera_last_mouse_y();

  double dx = xpos - last_x;
  double dy = last_y - ypos;
  hg_runtime_set_camera_mouse_state(0, xpos, ypos);

  dx *= kMouseSensitivity;
  dy *= kMouseSensitivity;

  const double yaw = hg_runtime_camera_yaw() + dx;
  const double pitch = std::clamp(hg_runtime_camera_pitch() + dy, -89.0, 89.0);
  updateCameraVectors(yaw, pitch);
}

}  // namespace hg
