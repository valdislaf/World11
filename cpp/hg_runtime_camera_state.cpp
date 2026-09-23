#include "hg_runtime_camera_state.hpp"

#include "hg_runtime_api.h"

namespace hg {

RuntimeCameraState& runtimeCameraState() {
  static RuntimeCameraState state;
  return state;
}

}  // namespace hg

extern "C" double hg_runtime_camera_x() {
  return hg::runtimeCameraState().pos[0];
}

extern "C" double hg_runtime_camera_y() {
  return hg::runtimeCameraState().pos[1];
}

extern "C" double hg_runtime_camera_z() {
  return hg::runtimeCameraState().pos[2];
}

extern "C" double hg_runtime_camera_front_x() {
  return hg::runtimeCameraState().front[0];
}

extern "C" double hg_runtime_camera_front_y() {
  return hg::runtimeCameraState().front[1];
}

extern "C" double hg_runtime_camera_front_z() {
  return hg::runtimeCameraState().front[2];
}

extern "C" double hg_runtime_camera_up_x() {
  return hg::runtimeCameraState().up[0];
}

extern "C" double hg_runtime_camera_up_y() {
  return hg::runtimeCameraState().up[1];
}

extern "C" double hg_runtime_camera_up_z() {
  return hg::runtimeCameraState().up[2];
}

extern "C" double hg_runtime_camera_yaw() {
  return hg::runtimeCameraState().yaw;
}

extern "C" double hg_runtime_camera_pitch() {
  return hg::runtimeCameraState().pitch;
}

extern "C" int hg_runtime_camera_first_mouse() {
  return hg::runtimeCameraState().first_mouse ? 1 : 0;
}

extern "C" double hg_runtime_camera_last_mouse_x() {
  return hg::runtimeCameraState().last_x;
}

extern "C" double hg_runtime_camera_last_mouse_y() {
  return hg::runtimeCameraState().last_y;
}

extern "C" void hg_runtime_set_camera_mouse_state(int first_mouse, double last_x, double last_y) {
  hg::RuntimeCameraState& state = hg::runtimeCameraState();
  state.first_mouse = first_mouse != 0;
  state.last_x = last_x;
  state.last_y = last_y;
}

extern "C" void hg_runtime_set_camera_orientation(double yaw,
                                                   double pitch,
                                                   double front_x,
                                                   double front_y,
                                                   double front_z,
                                                   double up_x,
                                                   double up_y,
                                                   double up_z) {
  hg::RuntimeCameraState& state = hg::runtimeCameraState();
  state.yaw = yaw;
  state.pitch = pitch;
  state.front[0] = front_x;
  state.front[1] = front_y;
  state.front[2] = front_z;
  state.up[0] = up_x;
  state.up[1] = up_y;
  state.up[2] = up_z;
}

extern "C" double hg_runtime_camera_speed() {
  return hg::runtimeCameraState().hud_speed;
}

extern "C" void hg_runtime_set_camera_speed(double speed) {
  hg::runtimeCameraState().hud_speed = speed;
}

extern "C" void hg_runtime_set_camera_position(double x, double y, double z) {
  hg::RuntimeCameraState& state = hg::runtimeCameraState();
  state.pos[0] = x;
  state.pos[1] = y;
  state.pos[2] = z;
  hg_universe_update_global_from_local(x, y, z);
}

extern "C" void hg_runtime_set_world_preserve_global(int world_id) {
  if (world_id < 1 || world_id > hg_world_count()) {
    return;
  }

  const double global_x = hg_runtime_global_camera_x();
  const double global_y = hg_runtime_global_camera_y();
  const double global_z = hg_runtime_global_camera_z();
  const double anchor_x = hg_runtime_world_anchor_x_at(world_id);
  const double anchor_y = hg_runtime_world_anchor_y_at(world_id);
  const double anchor_z = hg_runtime_world_anchor_z_at(world_id);

  hg_universe_set_current_world_only(world_id);
  hg_runtime_set_camera_position(global_x - anchor_x, global_y - anchor_y, global_z - anchor_z);
}
