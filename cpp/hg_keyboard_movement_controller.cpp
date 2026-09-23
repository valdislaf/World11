#include "hg_keyboard_movement_controller.hpp"

#include "hg_camera_movement_applicator.hpp"
#include "hg_camera_tuning.hpp"
#include "hg_runtime_api.h"
#include "hg_runtime_input_snapshot.hpp"

#include <GLFW/glfw3.h>

#include <cmath>

namespace hg {

namespace {

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

bool keyDown(int key) {
  return RuntimeInputSnapshot::keyState(key) == GLFW_PRESS;
}

}  // namespace

void KeyboardMovementController::updateFromInputSnapshot(double dt) {
  double speed_now = camera_tuning::kMoveSpeedBase;
  if (keyDown(GLFW_KEY_LEFT_SHIFT)) {
    speed_now = camera_tuning::kMoveSpeedBase * camera_tuning::kSprintMultiplier;
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

  if (keyDown(GLFW_KEY_W)) {
    displacement = displacement + velocity * front;
    moving = true;
  }
  if (keyDown(GLFW_KEY_S)) {
    displacement = displacement - velocity * front;
    moving = true;
  }
  if (keyDown(GLFW_KEY_A)) {
    displacement = displacement - velocity * right;
    moving = true;
  }
  if (keyDown(GLFW_KEY_D)) {
    displacement = displacement + velocity * right;
    moving = true;
  }

  const int world_id = hg_runtime_world();
  if (hg_runtime_is_cpp_vertical_motion_enabled(world_id) == 0) {
    if (keyDown(GLFW_KEY_SPACE)) {
      displacement = displacement + velocity * up;
      moving = true;
    }
    if (keyDown(GLFW_KEY_C)) {
      displacement = displacement - velocity * up;
      moving = true;
    }
  }

  CameraMovementApplicator::apply(CameraMovementDelta{displacement.x, displacement.y, displacement.z},
                                  moving,
                                  speed_now);
}

}  // namespace hg
