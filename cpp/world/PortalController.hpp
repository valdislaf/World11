#pragma once

#include "world/collision/CollisionTypes.hpp"

#include <cstddef>
#include <functional>

namespace hg::world {

/// <summary>A world-space trigger volume that teleports the camera to another world on contact.</summary>
struct Portal {
  float x;
  float y;
  float z;
  /// <summary>Trigger radius, in world units; combined with the camera radius to decide contact.</summary>
  float radius;
  /// <summary>1-based id of the world to switch to when this portal is triggered.</summary>
  int target_world_id;
  /// <summary>Camera position to place the player at in the target world, in that world's local coordinates.</summary>
  float entry_x;
  float entry_y;
  float entry_z;
};

/// <summary>Shared portal cooldown and contact-detection logic used by C++ worlds.</summary>
class PortalController final {
public:
  /// <summary>Default cooldown duration, in frames, applied after a successful portal switch.</summary>
  static constexpr int kDefaultCooldownFrames = 18;

  /// <summary>Decrements a per-world portal cooldown counter by one, clamped at zero.</summary>
  /// <param name="cooldown_frames">Remaining cooldown frames; modified in place.</param>
  static void tickCooldown(int& cooldown_frames);

  /// <summary>
  /// While the cooldown is at zero, checks the camera sphere against each portal's trigger
  /// sphere; on the first contact found, switches the active world, repositions the camera
  /// at the portal's entry point, invokes <paramref name="after_switch"/>, and resets the cooldown.
  /// </summary>
  /// <param name="portals">Pointer to the first element of the world's portal array. Not owned; must outlive the call and have at least <paramref name="portal_count"/> elements. Must not be <c>nullptr</c> if <paramref name="portal_count"/> is greater than zero.</param>
  /// <param name="portal_count">Number of portals in the <paramref name="portals"/> array.</param>
  /// <param name="camera_pos">Current camera position in world-local coordinates.</param>
  /// <param name="camera_radius">Camera collision sphere radius used for contact detection.</param>
  /// <param name="cooldown_frames">Remaining cooldown frames; read and, on a successful switch, reset to <paramref name="cooldown_reset_frames"/>.</param>
  /// <param name="cooldown_reset_frames">Cooldown duration, in frames, applied after a successful switch.</param>
  /// <param name="after_switch">Callback invoked immediately after switching world and repositioning the camera, before returning; may be empty.</param>
  /// <returns>True if a portal was triggered and the world was switched this call.</returns>
  /// <remarks>Has side effects on global runtime state via <c>RuntimeBridge</c> when a portal triggers.</remarks>
  static bool trySwitchRuntimePortal(const Portal* portals,
                                     std::size_t portal_count,
                                     const collision::Vec3& camera_pos,
                                     double camera_radius,
                                     int& cooldown_frames,
                                     int cooldown_reset_frames,
                                     const std::function<void()>& after_switch);
};

}  // namespace hg::world
