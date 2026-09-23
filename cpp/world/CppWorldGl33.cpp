#include "world/CppWorldGl33.hpp"

#include "hg_runtime_bridge.hpp"
#include "hg_runtime_frame_state.hpp"
#include "world/World11Seabed.hpp"
#include "world/World11WaterSurface.hpp"
#include "world/World11Landmarks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace hg::world {

namespace {

constexpr std::array<Portal, 1> kWorld6Portals = {
    Portal{0.0f, 0.0f, -30.0f, 1.5f, 11, 0.0f, 0.3f, -45.0f},
};

constexpr std::array<Portal, 1> kWorld7Portals = {
    Portal{0.0f, 0.0f, -4.0f, 1.25f, 6, -2.0f, 0.0f, -14.0f},
};

constexpr std::array<Portal, 1> kWorld8Portals = {
    Portal{0.0f, 0.0f, -6.0f, 1.25f, 6, -15.0f, 0.2f, -11.5f},
};

constexpr std::array<Portal, 1> kWorld9Portals = {
    Portal{0.0f, 0.0f, 9.0f, 1.15f, 6, -15.0f, 0.2f, -12.0f},
};

constexpr std::array<Portal, 1> kWorld10Portals = {
    Portal{0.0f, 0.0f, -14.0f, 1.25f, 6, 5.0f, 0.2f, -11.5f},
};

const std::array<Portal, 1> kWorld11Portals = {
    Portal{0.0f, kWorld11ReturnPortalY, -20.0f, 1.5f, 6, 0.0f, 0.3f, -45.0f},
};

constexpr std::array<Portal, 1> kWorld12Portals = {
    Portal{0.0f, 0.0f, -14.0f, 1.5f, 6, 10.0f, 0.3f, -26.0f},
};

const Portal* portalData(int worldId) {
  switch (worldId) {
    case 6: return kWorld6Portals.data();
    case 7: return kWorld7Portals.data();
    case 8: return kWorld8Portals.data();
    case 9: return kWorld9Portals.data();
    case 10: return kWorld10Portals.data();
    case 11: return kWorld11Portals.data();
    case 12: return kWorld12Portals.data();
    default: return nullptr;
  }
}

std::size_t portalCount(int worldId) {
  return worldId == 6 ? kWorld6Portals.size() :
      ((worldId >= 7 && worldId <= 12) ? 1u : 0u);
}

float world7Height(float x, float z) {
  const float radius = std::sqrt(x * x + (z + 50.0f) * (z + 50.0f));
  const float hill = std::max(0.0f, 1.0f - radius / 14.0f);
  return -1.0f + hill * hill * 9.5f +
      std::sin(x * 0.61f + z * 0.17f) * 0.18f * hill;
}

float world12Height(float x, float z) {
  const float smallRock =
      std::sin(x * 0.19f + z * 0.07f) * 0.24f +
      std::sin(-x * 0.08f + z * 0.17f + 1.3f) * 0.18f +
      std::sin(x * 0.42f - z * 0.31f + 0.7f) * 0.07f;
  const float broadRock = std::sin(x * 0.035f) * std::cos(z * 0.029f) * 0.48f;
  const float craterDistance = std::sqrt(x * x + (z + 112.0f) * (z + 112.0f));
  const float cone = std::max(0.0f, 1.0f - craterDistance / 80.0f) * 30.0f;
  const float rimOffset = (craterDistance - 18.0f) / 5.8f;
  const float rim = std::exp(-rimOffset * rimOffset) * 9.0f;
  const float craterRatio = craterDistance / 11.5f;
  const float bowl = std::exp(-craterRatio * craterRatio * craterRatio * craterRatio) * 4.0f;
  const float lakeX = (x - 24.0f) / 13.5f;
  const float lakeZ = (z + 36.0f) / 9.5f;
  const float lakeDistance = std::sqrt(lakeX * lakeX + lakeZ * lakeZ);
  const float basin = std::exp(-lakeDistance * lakeDistance * lakeDistance * lakeDistance) * 1.65f;
  return -1.0f + smallRock + broadRock + cone + rim - bowl - basin;
}

float world12WalkableHeight(float x, float z) {
  float height = world12Height(x, z);
  if (std::sqrt(x * x + (z + 112.0f) * (z + 112.0f)) < 11.5f) {
    height = std::max(height, 26.4f);
  }
  const float lakeX = (x - 24.0f) / 13.5f;
  const float lakeZ = (z + 36.0f) / 9.5f;
  if (lakeX * lakeX + lakeZ * lakeZ < 1.0f) {
    height = std::max(height, -2.30f);
  }
  return height;
}

void addFloor(collision::CollisionWorld& collisionWorld, const char* name,
              double topY, double halfExtent) {
  collisionWorld.addAabb(name,
      {-halfExtent, -10000.0, -halfExtent},
      {halfExtent, topY, halfExtent});
}

void addBox(collision::CollisionWorld& collisionWorld, const std::string& name,
            float x, float bottomY, float z,
            float sizeX, float sizeY, float sizeZ) {
  collisionWorld.addAabb(name,
      {x - sizeX * 0.5, bottomY, z - sizeZ * 0.5},
      {x + sizeX * 0.5, bottomY + sizeY, z + sizeZ * 0.5});
}

}  // namespace

CppWorldGl33::CppWorldGl33(int worldId)
    : BaseWorld(portalData(worldId), portalCount(worldId)),
      worldId_(worldId), renderer_(worldId) {
  if (worldId_ < 6 || worldId_ > 12) {
    throw std::invalid_argument("CppWorldGl33 supports world ids 6 through 12");
  }
  initColliders();
}

int CppWorldGl33::getWorldId() const {
  return worldId_;
}

void CppWorldGl33::initColliders() {
  switch (worldId_) {
    case 6:
      addBox(collisionWorld_, "pyramid_0", -4.0f, -1.0f, -100.0f, 4.8f, 3.0f, 4.8f);
      addBox(collisionWorld_, "pyramid_1", 4.0f, -1.0f, -10.0f, 4.8f, 3.0f, 4.8f);
      break;
    case 7:
      addFloor(collisionWorld_, "floor", -1.0, 500.0);
      addBox(collisionWorld_, "terrace", 0.0f, -1.0f, -9.0f, 12.0f, 0.6f, 12.0f);
      addBox(collisionWorld_, "art_complex", 17.0f, -1.0f, -11.5f, 10.0f, 0.55f, 8.0f);
      addBox(collisionWorld_, "recognizer_left", -12.0f, -1.0f, -42.0f, 6.0f, 18.0f, 8.0f);
      addBox(collisionWorld_, "recognizer_right", 12.0f, -1.0f, -42.0f, 6.0f, 18.0f, 8.0f);
      break;
    case 8:
      addFloor(collisionWorld_, "floor", -1.0, 80.0);
      break;
    case 9:
      collisionWorld_.addAabb("floor", {-8.4, -10000.0, -70.0}, {8.4, -1.0, 12.0});
      collisionWorld_.addAabb("ceiling", {-8.4, 2.45, -70.0}, {8.4, 10000.0, 12.0});
      collisionWorld_.addAabb("left_wall", {-10000.0, -1.0, -44.0}, {-2.55, 2.45, 12.0});
      collisionWorld_.addAabb("right_wall", {2.55, -1.0, -44.0}, {10000.0, 2.45, 12.0});
      break;
    case 10:
      addFloor(collisionWorld_, "floor", -1.0, 60.0);
      addBox(collisionWorld_, "spire", 0.0f, -1.0f, -32.0f, 3.4f, 9.0f, 3.4f);
      break;
    case 11: {
      const auto arch = kWorld11Arch;
      const float baseY = world11SeabedHeight(arch.x, arch.z);
      for (int i = 0; i <= 14; ++i) {
        const float angle = 3.14159265f * i / 14.0f;
        const float x = arch.x + 6.0f * std::cos(angle);
        const float y = baseY + 1.2f + 8.0f * std::sin(angle);
        const float z = arch.z + 0.28f * std::sin(i * 1.7f);
        // Inscribed boxes preserve the swim-through opening.
        addBox(collisionWorld_, "reef_arch_" + std::to_string(i),
               x, y - 1.0f, z, 1.5f, 2.0f, 1.8f);
      }
      break;
    }
    case 12:
      for (int index = 0; index < 12; ++index) {
        constexpr std::array<std::array<float, 3>, 12> columns = {{
            {-18.0f, 5.2f, -24.0f}, {-21.0f, 6.4f, -29.0f},
            {38.0f, 7.0f, -54.0f}, {35.0f, 8.2f, -58.0f},
            {-42.0f, 8.8f, -67.0f}, {-40.0f, 7.1f, -75.0f},
            {27.0f, 6.8f, -86.0f}, {31.0f, 9.3f, -89.0f},
            {-31.0f, 9.0f, -105.0f}, {25.0f, 8.0f, -121.0f},
            {22.0f, 6.4f, -126.0f}, {-15.0f, 7.5f, -139.0f},
        }};
        const auto& column = columns[static_cast<std::size_t>(index)];
        addBox(collisionWorld_, "basalt_" + std::to_string(index),
               column[0], world12Height(column[0], column[2]), column[2],
               1.8f, column[1], 1.8f);
      }
      break;
    default:
      break;
  }
}

void CppWorldGl33::beforeBeginFrame(Engine&, double dt) {
  const float frameDt = static_cast<float>((dt > 0.0 && dt < 0.08)
      ? dt : (1.0 / 60.0));
  sceneTime_ += frameDt;
}

double CppWorldGl33::groundContactAt(double x, double z) const {
  switch (worldId_) {
    case 7: return static_cast<double>(world7Height(static_cast<float>(x), static_cast<float>(z)));
    case 9: return -0.30;
    case 11: return static_cast<double>(world11SeabedHeight(
        static_cast<float>(x), static_cast<float>(z)));
    case 12: return static_cast<double>(world12WalkableHeight(
        static_cast<float>(x), static_cast<float>(z)));
    default: return -1.0;
  }
}

double CppWorldGl33::cameraRadius() const {
  return worldId_ == 9 ? 0.45 : BaseWorld::cameraRadius();
}

void CppWorldGl33::updateDoor(const collision::Vec3& cameraPosition, double dt) {
  if (worldId_ != 9) {
    return;
  }
  constexpr float kDoorZ = -43.95f;
  const double dx = cameraPosition[0];
  const double dz = cameraPosition[2] - static_cast<double>(kDoorZ);
  const bool nearDoor = dx * dx + dz * dz <= 8.4 * 8.4;
  const float target = nearDoor ? 1.0f : 0.0f;
  const float speed = nearDoor ? 0.42f : 0.30f;
  const float step = speed * static_cast<float>(dt);
  if (doorOpenAmount_ < target) {
    doorOpenAmount_ = std::min(target, doorOpenAmount_ + step);
  } else {
    doorOpenAmount_ = std::max(target, doorOpenAmount_ - step);
  }
}

collision::Vec3 CppWorldGl33::applyDoorBlock(
    const collision::Vec3& cameraPosition, const collision::Vec3& resolved) const {
  if (worldId_ == 11) {
    collision::Vec3 adjusted = resolved;
    const double floorY = groundContactAt(adjusted[0], adjusted[2]) + cameraRadius();
    const double ceilingY = static_cast<double>(world11WaterSurfaceHeight(
        static_cast<float>(adjusted[0]), static_cast<float>(adjusted[2]),
        sceneTime_)) - cameraRadius() - 0.15;
    adjusted[1] = std::clamp(adjusted[1], floorY, std::max(floorY, ceilingY));
    return adjusted;
  }
  if (worldId_ != 9 || doorOpenAmount_ >= 0.72f) {
    return resolved;
  }
  collision::Vec3 adjusted = resolved;
  constexpr double kDoorBlockZ = -43.18;
  if (cameraPosition[2] > kDoorBlockZ && resolved[2] < kDoorBlockZ &&
      std::abs(resolved[0]) < 1.95) {
    adjusted[2] = kDoorBlockZ;
  }
  return adjusted;
}

void CppWorldGl33::drawScene() const {
  const render::gl33::Gl33WorldFrame frame = {
      worldId_,
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
      sceneTime_,
      doorOpenAmount_,
      portalData(worldId_),
      portalCount(worldId_),
  };
  renderer_.render(frame);
}

void CppWorldGl33::drawPortals() const {
  // Portals are part of the Core scene batch and are drawn by Gl33WorldRenderer.
}

}  // namespace hg::world
