#include "hg_runtime_universe.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace hg {

namespace {

constexpr double kEarthRadiusMeters = 6.371e6;
constexpr std::array<double, 3> kEarthSystemPositionWorld1 = {
    -25484000.0,
    0.0,
    -25484000.0,
};

struct UniverseWorld {
  std::array<double, 3> anchor = {0.0, 0.0, 0.0};
  int seed = 0;
};

struct UniverseState {
  std::vector<UniverseWorld> worlds;
  int current_world = 1;
  std::array<double, 3> cam_global = {0.0, 0.0, 0.0};
  int configured_world_count = 0;
};

UniverseState& universeState() {
  static UniverseState state;
  return state;
}

double length(const std::array<double, 3>& v) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

std::array<double, 3> subtract(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

std::array<double, 3> scale(const std::array<double, 3>& v, double factor) {
  return {v[0] * factor, v[1] * factor, v[2] * factor};
}

void ensureInitialized() {
  if (universeState().worlds.empty()) {
    RuntimeUniverse::init();
  }
}

}  // namespace

void RuntimeUniverse::configureWorldCount(int world_count) {
  auto& state = universeState();
  state.configured_world_count = std::max(1, world_count);
  state.worlds.clear();
}

void RuntimeUniverse::init() {
  auto& state = universeState();
  const int world_count = state.configured_world_count > 0 ? state.configured_world_count : 5;

  if (state.worlds.empty()) {
    state.worlds.assign(static_cast<std::size_t>(world_count), UniverseWorld{});
  }

  if (state.current_world < 1 || state.current_world > world_count) {
    state.current_world = 1;
  }

  state.worlds[0].anchor = {0.0, 0.0, 0.0};
  state.worlds[0].seed = 1;

  const std::array<double, 3> to_earth = subtract(kEarthSystemPositionWorld1, state.worlds[0].anchor);
  const double to_earth_distance = length(to_earth);
  const std::array<double, 3> direction =
      to_earth_distance > 0.0 ? scale(to_earth, 1.0 / to_earth_distance) : std::array<double, 3>{0.0, 0.0, 0.0};

  for (int i = 2; i <= world_count; ++i) {
    const double offset_distance = kEarthRadiusMeters + std::max(1000000.0, 22000000.0 - static_cast<double>(i - 2) * 4000000.0);
    UniverseWorld& world = state.worlds[static_cast<std::size_t>(i - 1)];
    world.anchor = subtract(kEarthSystemPositionWorld1, scale(direction, offset_distance));
    world.seed = i;
  }

  if (world_count >= 5) {
    state.worlds[4].anchor = {-2.029e7, -1.17e6, -4.085e8};
  }

  if (world_count >= 9) {
    const auto world8_anchor = state.worlds[7].anchor;
    for (int i = 9; i <= world_count; ++i) {
      UniverseWorld& world = state.worlds[static_cast<std::size_t>(i - 1)];
      world.anchor = {
          world8_anchor[0] + static_cast<double>(i - 8) * 4000000.0,
          world8_anchor[1],
          world8_anchor[2],
      };
    }
  }

  updateGlobalFromLocal(0.0, 0.0, 0.0);
}

int RuntimeUniverse::currentWorld() {
  ensureInitialized();
  return universeState().current_world;
}

int RuntimeUniverse::worldCount() {
  auto& state = universeState();
  if (state.worlds.empty()) {
    if (state.configured_world_count > 0) {
      return state.configured_world_count;
    }
    RuntimeUniverse::init();
  }
  return static_cast<int>(state.worlds.size());
}

void RuntimeUniverse::switchWorld(int world_id) {
  ensureInitialized();
  auto& state = universeState();
  if (world_id < 1 || world_id > static_cast<int>(state.worlds.size())) {
    return;
  }
  state.current_world = world_id;
  state.cam_global = state.worlds[static_cast<std::size_t>(world_id - 1)].anchor;
}

void RuntimeUniverse::setCurrentWorldOnly(int world_id) {
  ensureInitialized();
  auto& state = universeState();
  if (world_id < 1 || world_id > static_cast<int>(state.worlds.size())) {
    return;
  }
  state.current_world = world_id;
}

void RuntimeUniverse::updateGlobalFromLocal(double x, double y, double z) {
  ensureInitialized();
  auto& state = universeState();
  const auto& anchor = state.worlds[static_cast<std::size_t>(state.current_world - 1)].anchor;
  state.cam_global = {anchor[0] + x, anchor[1] + y, anchor[2] + z};
}

std::array<double, 3> RuntimeUniverse::globalCamera() {
  ensureInitialized();
  return universeState().cam_global;
}

std::array<double, 3> RuntimeUniverse::worldAnchor(int world_id) {
  ensureInitialized();
  const auto& state = universeState();
  if (world_id < 1 || world_id > static_cast<int>(state.worlds.size())) {
    return {0.0, 0.0, 0.0};
  }
  return state.worlds[static_cast<std::size_t>(world_id - 1)].anchor;
}

}  // namespace hg

extern "C" void hg_universe_init() {
  hg::RuntimeUniverse::init();
}

extern "C" int hg_world_count() {
  return hg::RuntimeUniverse::worldCount();
}

extern "C" void hg_universe_update_global_from_local(double x, double y, double z) {
  hg::RuntimeUniverse::updateGlobalFromLocal(x, y, z);
}

extern "C" void hg_universe_set_current_world_only(int world_id) {
  hg::RuntimeUniverse::setCurrentWorldOnly(world_id);
}

extern "C" void hg_runtime_configure_world_count(int world_count) {
  hg::RuntimeUniverse::configureWorldCount(world_count);
}

extern "C" void hg_runtime_set_world(int world_id) {
  hg::RuntimeUniverse::switchWorld(world_id);
}

extern "C" int hg_runtime_world() {
  return hg::RuntimeUniverse::currentWorld();
}

extern "C" double hg_runtime_global_camera_x() {
  return hg::RuntimeUniverse::globalCamera()[0];
}

extern "C" double hg_runtime_global_camera_y() {
  return hg::RuntimeUniverse::globalCamera()[1];
}

extern "C" double hg_runtime_global_camera_z() {
  return hg::RuntimeUniverse::globalCamera()[2];
}

extern "C" double hg_runtime_world_anchor_x() {
  return hg::RuntimeUniverse::worldAnchor(hg::RuntimeUniverse::currentWorld())[0];
}

extern "C" double hg_runtime_world_anchor_y() {
  return hg::RuntimeUniverse::worldAnchor(hg::RuntimeUniverse::currentWorld())[1];
}

extern "C" double hg_runtime_world_anchor_z() {
  return hg::RuntimeUniverse::worldAnchor(hg::RuntimeUniverse::currentWorld())[2];
}

extern "C" double hg_runtime_world_anchor_x_at(int world_id) {
  return hg::RuntimeUniverse::worldAnchor(world_id)[0];
}

extern "C" double hg_runtime_world_anchor_y_at(int world_id) {
  return hg::RuntimeUniverse::worldAnchor(world_id)[1];
}

extern "C" double hg_runtime_world_anchor_z_at(int world_id) {
  return hg::RuntimeUniverse::worldAnchor(world_id)[2];
}
