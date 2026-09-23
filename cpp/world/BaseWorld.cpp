#include "world/BaseWorld.hpp"

#include "hg_engine.hpp"
#include "hg_runtime_bridge.hpp"
#include "world/PortalController.hpp"

namespace hg::world {

BaseWorld::BaseWorld() = default;

BaseWorld::BaseWorld(const Portal* portals, std::size_t portalCount)
    : portals_(portals), portalCount_(portalCount) {
}

void BaseWorld::tick(Engine& engine, double dt) {
  beforeWorldCheck(engine, dt);

  const bool tickCooldownBeforeWorldCheck = shouldTickPortalCooldownBeforeWorldCheck();
  if (tickCooldownBeforeWorldCheck) {
    PortalController::tickCooldown(portalCooldownFrames_);
  }

  if (RuntimeBridge::currentWorld() != getWorldId()) {
    tickInactiveWorld(engine, dt);
    return;
  }

  beforeBeginFrame(engine, dt);
  if (!tickCooldownBeforeWorldCheck) {
    PortalController::tickCooldown(portalCooldownFrames_);
  }
  RuntimeBridge::beginFrame();
  afterBeginFrame(engine, dt);

  const collision::Vec3 resolved = doPhysicsTick(engine, dt);
  if (handlePortals(resolved, engine, dt)) {
    beforeEndFrame(engine, dt);
    RuntimeBridge::endFrame();
    engine.update(dt);
    return;
  }

  drawScene();
  drawPortals();

  beforeEndFrame(engine, dt);
  RuntimeBridge::endFrame();
  engine.update(dt);
}

void BaseWorld::beforeWorldCheck(Engine&, double) {
}

bool BaseWorld::shouldTickPortalCooldownBeforeWorldCheck() const {
  return true;
}

void BaseWorld::tickInactiveWorld(Engine& engine, double dt) {
  RuntimeBridge::beginFrame();
  RuntimeBridge::endFrame();
  resetMotionState();
  engine.update(dt);
}

void BaseWorld::beforeBeginFrame(Engine&, double) {
}

void BaseWorld::afterBeginFrame(Engine&, double) {
  enableVerticalPhysics_ = RuntimeBridge::isWorldGravityPhysicsEnabled(getWorldId());
}

collision::Vec3 BaseWorld::doPhysicsTick(Engine&, double dt) {
  const collision::Vec3 camera_pos = {
      RuntimeBridge::cameraX(),
      RuntimeBridge::cameraY(),
      RuntimeBridge::cameraZ(),
  };
  updateDoor(camera_pos, dt);

  if (!prevCameraValid_) {
    resetMotionState();
    prevCameraPos_ = camera_pos;
    prevCameraValid_ = true;
    verticalMotion_.initialize(camera_pos);
  }

  const auto groundContact = [this](double x, double z) -> double {
    return groundContactAt(x, z);
  };

  const collision::Vec3 physics_resolved = verticalMotion_.resolve(prevCameraPos_,
                                                                    camera_pos,
                                                                    dt,
                                                                    enableVerticalPhysics_,
                                                                    RuntimeBridge::isSpacePressed(),
                                                                    groundContact,
                                                                    collisionResolver_,
                                                                    collisionWorld_,
                                                                    cameraRadius());
  const collision::Vec3 resolved = applyDoorBlock(camera_pos, physics_resolved);

  if (resolved != camera_pos) {
    RuntimeBridge::setCameraPosition(resolved[0], resolved[1], resolved[2]);
  }

  prevCameraPos_ = resolved;
  return resolved;
}

double BaseWorld::groundContactAt(double, double) const {
  return static_cast<double>(kFloorY);
}

double BaseWorld::cameraRadius() const {
  return kCameraRadius;
}

bool BaseWorld::handlePortals(const collision::Vec3& resolved, Engine&, double) {
  if (portalCount_ == 0) {
    return false;
  }

  return PortalController::trySwitchRuntimePortal(portals_,
                                                  portalCount_,
                                                  resolved,
                                                  cameraRadius(),
                                                  portalCooldownFrames_,
                                                  PortalController::kDefaultCooldownFrames,
                                                  [this] { resetMotionState(); });
}

void BaseWorld::drawPortals() const {
}

void BaseWorld::beforeEndFrame(Engine&, double) {
}

void BaseWorld::updateDoor(const collision::Vec3&, double) {
}

collision::Vec3 BaseWorld::applyDoorBlock(const collision::Vec3&, const collision::Vec3& resolved) const {
  return resolved;
}

void BaseWorld::resetMotionState() {
  prevCameraValid_ = false;
  verticalMotion_.reset();
}

}  // namespace hg::world
