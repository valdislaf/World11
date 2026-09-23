#include "hg_runtime_frame_state.hpp"

#include <algorithm>

namespace hg {

namespace {

double g_dt = 1.0e-6;
int g_framebuffer_width = 1;
int g_framebuffer_height = 1;

}  // namespace

void RuntimeFrameState::setFrame(double dt, int framebuffer_width, int framebuffer_height) {
  g_dt = std::max(1.0e-6, dt);
  g_framebuffer_width = std::max(1, framebuffer_width);
  g_framebuffer_height = std::max(1, framebuffer_height);
}

int RuntimeFrameState::framebufferWidth() {
  return g_framebuffer_width;
}

int RuntimeFrameState::framebufferHeight() {
  return g_framebuffer_height;
}

}  // namespace hg
