#include "hg_frame_title_controller.hpp"

#include "hg_glfw_runtime_window.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>

namespace hg {

namespace {

int g_fps_frames = 0;
double g_fps_t0 = 0.0;

}  // namespace

void FrameTitleController::reset() {
  g_fps_frames = 0;
  g_fps_t0 = glfwGetTime();
}

void FrameTitleController::update() {
  auto* window = GlfwRuntimeWindow::window();
  if (window == nullptr) {
    return;
  }

  const double now = glfwGetTime();
  if (g_fps_t0 <= 0.0) {
    g_fps_t0 = now;
  }

  ++g_fps_frames;
  const double elapsed = now - g_fps_t0;
  if (elapsed < 1.0) {
    return;
  }

  char title[64] = {};
  std::snprintf(title, sizeof(title), "Horizon Gates - FPS: %6.1f", static_cast<double>(g_fps_frames) / elapsed);
  glfwSetWindowTitle(window, title);
  g_fps_frames = 0;
  g_fps_t0 = now;
}

}  // namespace hg
