#include "world/PortalController.hpp"

#include "hg_runtime_bridge.hpp"
namespace hg::world {

void PortalController::tickCooldown(int& cooldown_frames) {
  if (cooldown_frames > 0) {
    --cooldown_frames;
  }
}

bool PortalController::trySwitchRuntimePortal(const Portal* portals,
                                              std::size_t portal_count,
                                              const collision::Vec3& camera_pos,
                                              double camera_radius,
                                              int& cooldown_frames,
                                              int cooldown_reset_frames,
                                              const std::function<void()>& after_switch) {
  if (cooldown_frames != 0) {
    return false;
  }

  for (std::size_t i = 0; i < portal_count; ++i) {
    const Portal& portal = portals[i];
    const double dx = camera_pos[0] - static_cast<double>(portal.x);
    const double dy = camera_pos[1] - static_cast<double>(portal.y);
    const double dz = camera_pos[2] - static_cast<double>(portal.z);
    const double trigger_radius = static_cast<double>(portal.radius) + camera_radius;
    if (dx * dx + dy * dy + dz * dz > trigger_radius * trigger_radius) {
      continue;
    }

    RuntimeBridge::switchWorld(portal.target_world_id);
    RuntimeBridge::setCameraPosition(portal.entry_x, portal.entry_y, portal.entry_z);
    after_switch();
    cooldown_frames = cooldown_reset_frames;
    return true;
  }

  return false;
}

}  // namespace hg::world
