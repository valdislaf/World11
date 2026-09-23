#include "hg_runtime_world_flags.hpp"

#include <algorithm>
#include <vector>

namespace hg {

namespace {

std::vector<bool>& verticalMotionFlags() {
  static std::vector<bool> flags(1, false);
  return flags;
}

}  // namespace

void RuntimeWorldFlags::configure(int world_count) {
  const int count = std::max(1, world_count);
  verticalMotionFlags().assign(static_cast<std::size_t>(count), false);
}

void RuntimeWorldFlags::setCppVerticalMotion(int world_id, bool enabled) {
  if (world_id < 1) {
    return;
  }

  auto& flags = verticalMotionFlags();
  if (world_id > static_cast<int>(flags.size())) {
    flags.resize(static_cast<std::size_t>(world_id), false);
  }
  flags[static_cast<std::size_t>(world_id - 1)] = enabled;
}

bool RuntimeWorldFlags::isCppVerticalMotion(int world_id) {
  const auto& flags = verticalMotionFlags();
  if (world_id < 1 || world_id > static_cast<int>(flags.size())) {
    return false;
  }
  return flags[static_cast<std::size_t>(world_id - 1)];
}

}  // namespace hg

extern "C" void hg_runtime_configure_world_flags(int world_count) {
  hg::RuntimeWorldFlags::configure(world_count);
}

extern "C" void hg_runtime_set_cpp_vertical_motion(int world_id, int enabled) {
  hg::RuntimeWorldFlags::setCppVerticalMotion(world_id, enabled != 0);
}

extern "C" int hg_runtime_is_cpp_vertical_motion_enabled(int world_id) {
  return hg::RuntimeWorldFlags::isCppVerticalMotion(world_id) ? 1 : 0;
}
