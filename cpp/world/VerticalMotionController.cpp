#include "world/VerticalMotionController.hpp"

#include <cmath>

namespace hg::world {

namespace {

constexpr double kCameraHeightOffset = 0.20;
constexpr double kGravity = -14.0;
constexpr double kJumpImpulse = 8.0;
constexpr double kGroundEpsilon = 1e-4;
constexpr double kJumpGroundThreshold = 0.08;
constexpr double kJumpBufferSeconds = 0.18;
constexpr double kCoyoteSeconds = 0.10;

}  // namespace

void VerticalMotionController::reset() {
  verticalVelocity_ = 0.0;
  physicsY_ = 0.0;
  initialized_ = false;
  onGround_ = false;
  jumpInputHeld_ = false;
  jumpBufferS_ = 0.0;
  coyoteS_ = 0.0;
}

void VerticalMotionController::initialize(const collision::Vec3& camera_pos) {
  physicsY_ = camera_pos[1];
  verticalVelocity_ = 0.0;
  onGround_ = false;
  initialized_ = true;
}

collision::Vec3 VerticalMotionController::resolve(const collision::Vec3& previous_camera_pos,
                                                  const collision::Vec3& camera_pos,
                                                  double dt,
                                                  bool physics_enabled,
                                                  bool jump_down,
                                                  const std::function<double(double, double)>& ground_contact_at,
                                                  collision::CollisionResolver& resolver,
                                                  const collision::CollisionWorld& collision_world,
                                                  double camera_radius) {
  if (!initialized_) {
    initialize(camera_pos);
  }

  if (!physics_enabled) {
    physicsY_ = camera_pos[1];
    verticalVelocity_ = 0.0;
    onGround_ = false;
    jumpBufferS_ = 0.0;
    coyoteS_ = 0.0;
    jumpInputHeld_ = jump_down;
    return resolver.resolveMovement(previous_camera_pos, camera_pos, camera_radius, collision_world);
  }

  const double ground_contact_current = ground_contact_at(camera_pos[0], camera_pos[2]) + camera_radius + kCameraHeightOffset;
  const bool near_ground = physicsY_ <= (ground_contact_current + kJumpGroundThreshold);
  const bool jump_pressed = jump_down && !jumpInputHeld_;
  jumpInputHeld_ = jump_down;

  if (jump_pressed) {
    jumpBufferS_ = kJumpBufferSeconds;
  }
  if (jumpBufferS_ > 0.0) {
    jumpBufferS_ -= dt;
    if (jumpBufferS_ < 0.0) {
      jumpBufferS_ = 0.0;
    }
  }

  if (onGround_ || near_ground) {
    coyoteS_ = kCoyoteSeconds;
  } else if (coyoteS_ > 0.0) {
    coyoteS_ -= dt;
    if (coyoteS_ < 0.0) {
      coyoteS_ = 0.0;
    }
  }

  if (jumpBufferS_ > 0.0 && coyoteS_ > 0.0) {
    verticalVelocity_ = kJumpImpulse;
    onGround_ = false;
    jumpBufferS_ = 0.0;
    coyoteS_ = 0.0;
  }

  verticalVelocity_ += kGravity * dt;
  physicsY_ += verticalVelocity_ * dt;

  collision::Vec3 desired = camera_pos;
  desired[1] = physicsY_;

  collision::Vec3 resolved =
      resolver.resolveMovement(previous_camera_pos, desired, camera_radius, collision_world);

  const double ground_contact_resolved = ground_contact_at(resolved[0], resolved[2]) + camera_radius + kCameraHeightOffset;
  bool forced_ground = false;
  if (resolved[1] < ground_contact_resolved) {
    resolved[1] = ground_contact_resolved;
    verticalVelocity_ = 0.0;
    onGround_ = true;
    forced_ground = true;
  }

  if (forced_ground || std::abs(resolved[1] - ground_contact_resolved) <= kGroundEpsilon) {
    onGround_ = true;
  } else if (resolved[1] > desired[1] && verticalVelocity_ < 0.0) {
    verticalVelocity_ = 0.0;
    onGround_ = true;
  } else if (resolved[1] < desired[1] && verticalVelocity_ > 0.0) {
    verticalVelocity_ = 0.0;
    onGround_ = false;
  } else {
    onGround_ = false;
  }

  physicsY_ = resolved[1];
  return resolved;
}

}  // namespace hg::world
