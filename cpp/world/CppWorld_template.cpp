#include "world/CppWorld_template.hpp"

#include "hg_runtime_bridge.hpp"
#include "hg_runtime_frame_state.hpp"

#include <array>

namespace hg::world {

namespace {

// Replace this invalid placeholder with a unique registered id after copying the template.
// BaseWorld provides the current lifecycle, collision, physics, and portal flow used by production worlds.
constexpr int kTemplateWorldId = -1;
constexpr double kFloorTopY = -1.0;
constexpr float kFloorHalfExtent = 64.0f;

constexpr std::array<Portal, 1> kTemplatePortals = {
    Portal{0.0f, 0.0f, -6.0f, 1.25f, 6, 0.0f, 0.0f, -14.0f},
};

collision::Aabb makeFloorAabb() {
  return collision::Aabb{
      {-static_cast<double>(kFloorHalfExtent), -10000.0, -static_cast<double>(kFloorHalfExtent)},
      {static_cast<double>(kFloorHalfExtent), kFloorTopY, static_cast<double>(kFloorHalfExtent)},
  };
}

}  // namespace

CppWorldTemplate::CppWorldTemplate()
    : BaseWorld(kTemplatePortals.data(), kTemplatePortals.size()),
      renderer_(kTemplateWorldId) {
  initColliders();
}

void CppWorldTemplate::initColliders() {
  const collision::Aabb floor = makeFloorAabb();
  collisionWorld_.addAabb("floor", floor.min, floor.max);

  // Add world-local colliders here, for example:
  // collisionWorld_.addAabb("block_0", {-1.0, -1.0, -8.0}, {1.0, 1.0, -6.0});
}

int CppWorldTemplate::getWorldId() const {
  return kTemplateWorldId;
}

void CppWorldTemplate::beforeBeginFrame(Engine&, double dt) {
  sceneTime_ += static_cast<float>((dt > 0.0 && dt < 0.08) ? dt : (1.0 / 60.0));
}

void CppWorldTemplate::drawScene() const {
  const render::gl33::Gl33WorldFrame frame = {
      kTemplateWorldId,
      {{static_cast<float>(RuntimeBridge::cameraX()),
        static_cast<float>(RuntimeBridge::cameraY()),
        static_cast<float>(RuntimeBridge::cameraZ())},
       {static_cast<float>(RuntimeBridge::cameraFrontX()),
        static_cast<float>(RuntimeBridge::cameraFrontY()),
        static_cast<float>(RuntimeBridge::cameraFrontZ())},
       {static_cast<float>(RuntimeBridge::cameraUpX()),
        static_cast<float>(RuntimeBridge::cameraUpY()),
        static_cast<float>(RuntimeBridge::cameraUpZ())},
       RuntimeFrameState::framebufferWidth(),
       RuntimeFrameState::framebufferHeight()},
      sceneTime_, 0.0f, kTemplatePortals.data(), kTemplatePortals.size(),
  };
  renderer_.render(frame);
}

}  // namespace hg::world
