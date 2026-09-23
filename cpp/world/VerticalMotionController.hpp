#pragma once

#include "world/collision/CollisionResolver.hpp"
#include "world/collision/CollisionWorld.hpp"

#include <functional>

namespace hg::world {

/// <summary>Shared jump/gravity controller for worlds with vertical physics enabled.</summary>
class VerticalMotionController {
public:
  /// <summary>Resets cached physics state after world switches/teleports.</summary>
  void reset();

  /// <summary>Synchronizes controller with current camera position before first use.</summary>
  /// <param name="camera_pos">Current camera position; its Y becomes the initial physics height.</param>
  void initialize(const collision::Vec3& camera_pos);

  /// <summary>
  /// If <paramref name="physics_enabled"/> is false, simply resolves horizontal/vertical
  /// movement via collision (no gravity). Otherwise integrates gravity, applies jump impulse
  /// (with input buffering and coyote time), resolves collision against the ground and
  /// colliders, and clamps the result to stand on the ground contact height.
  /// </summary>
  /// <param name="previous_camera_pos">Camera position at the start of the previous resolved step.</param>
  /// <param name="camera_pos">Camera position requested for this frame (before vertical physics is applied).</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <param name="physics_enabled">True to apply gravity/jump physics; false to only resolve movement as given.</param>
  /// <param name="jump_down">True if the jump input is currently held this frame.</param>
  /// <param name="ground_contact_at">
  /// Callback returning the ground height at a given (x, z) world position. Not owned; must
  /// remain valid for the duration of this call.
  /// </param>
  /// <param name="resolver">Collision resolver used to clip movement against <paramref name="collision_world"/>. Not owned.</param>
  /// <param name="collision_world">Collider storage for the active world. Not owned.</param>
  /// <param name="camera_radius">Camera collision sphere radius.</param>
  /// <returns>Resolved camera position to apply for this frame.</returns>
  collision::Vec3 resolve(const collision::Vec3& previous_camera_pos,
                          const collision::Vec3& camera_pos,
                          double dt,
                          bool physics_enabled,
                          bool jump_down,
                          const std::function<double(double, double)>& ground_contact_at,
                          collision::CollisionResolver& resolver,
                          const collision::CollisionWorld& collision_world,
                          double camera_radius);

private:
  double verticalVelocity_ = 0.0;
  double physicsY_ = 0.0;
  bool initialized_ = false;
  bool onGround_ = false;
  bool jumpInputHeld_ = false;
  double jumpBufferS_ = 0.0;
  double coyoteS_ = 0.0;
};

}  // namespace hg::world
