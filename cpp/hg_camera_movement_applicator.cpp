#include "hg_camera_movement_applicator.hpp"

#include "hg_runtime_api.h"
#include "hg_runtime_bridge.hpp"

#include <limits>

namespace hg {

namespace {

struct Vec3 {
  double x;
  double y;
  double z;
};

double distanceSquared(const Vec3& a, const Vec3& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  const double dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

int detectWorldFromGlobalCamera() {
  const Vec3 global_camera{
      hg_runtime_global_camera_x(),
      hg_runtime_global_camera_y(),
      hg_runtime_global_camera_z(),
  };

  int detected_world = 1;
  double best_distance = std::numeric_limits<double>::max();
  const int world_count = hg_world_count();

  for (int world_id = 1; world_id <= world_count; ++world_id) {
    const Vec3 anchor{
        hg_runtime_world_anchor_x_at(world_id),
        hg_runtime_world_anchor_y_at(world_id),
        hg_runtime_world_anchor_z_at(world_id),
    };
    const double distance = distanceSquared(global_camera, anchor);
    if (distance < best_distance) {
      best_distance = distance;
      detected_world = world_id;
    }
  }

  return detected_world;
}

void syncWorldFromGlobalCamera() {
  const int detected_world = detectWorldFromGlobalCamera();
  if (detected_world != hg_runtime_world()) {
    hg_runtime_set_world_preserve_global(detected_world);
  }
}

}  // namespace

void CameraMovementApplicator::apply(const CameraMovementDelta& displacement, bool moving, double speed_now) {
  if (moving) {
    RuntimeBridge::setCameraPosition(RuntimeBridge::cameraX() + displacement.x,
                                          RuntimeBridge::cameraY() + displacement.y,
                                          RuntimeBridge::cameraZ() + displacement.z);
    hg_runtime_set_camera_speed(speed_now);
  } else {
    hg_runtime_set_camera_speed(0.0);
  }

  syncWorldFromGlobalCamera();
}

}  // namespace hg
