#include "hg_runtime_bridge.hpp"

#include "hg_frame_title_controller.hpp"
#include "hg_gl33_runtime_state.hpp"
#include "hg_runtime_api.h"
#include "hg_gamepad_controller.hpp"
#include "hg_glfw_runtime_window.hpp"
#include "hg_keyboard_movement_controller.hpp"
#include "hg_mouse_look_controller.hpp"
#include "hg_runtime_frame_state.hpp"
#include "hg_runtime_input_snapshot.hpp"
#include "render/gl33/Gl33HudRenderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace hg {

namespace {

constexpr double kMinimumFrameDt = 1.0e-6;

double g_previous_frame_time = 0.0;
bool g_frame_clock_ready = false;
void resetFrameClock() {
  g_previous_frame_time = glfwGetTime();
  g_frame_clock_ready = true;
}

void clearFrameClock() {
  g_previous_frame_time = 0.0;
  g_frame_clock_ready = false;
}

double updateRuntimeFrameState() {
  const double now = glfwGetTime();
  if (!g_frame_clock_ready) {
    g_previous_frame_time = now;
    g_frame_clock_ready = true;
  }

  double frame_dt = now - g_previous_frame_time;
  g_previous_frame_time = now;
  if (frame_dt <= 0.0) {
    frame_dt = kMinimumFrameDt;
  }

  int width = 1;
  int height = 1;
  auto* window = GlfwRuntimeWindow::window();
  if (window != nullptr) {
    glfwGetFramebufferSize(window, &width, &height);
  }
  if (width <= 0) {
    width = 1;
  }
  if (height <= 0) {
    height = 1;
  }

  RuntimeFrameState::setFrame(frame_dt, width, height);
  return frame_dt;
}

}  // namespace

void RuntimeBridge::configureRuntimeWorldCount(int world_count) {
  hg_runtime_configure_world_count(world_count);
  hg_runtime_configure_world_flags(world_count);
}

void RuntimeBridge::initRuntime() {
  GlfwRuntimeWindow::initialize();
  resetFrameClock();
  hg_universe_init();
  hg_runtime_set_camera_position(hg_runtime_camera_x(), hg_runtime_camera_y(), hg_runtime_camera_z());
  Gl33RuntimeState::initialize();
  FrameTitleController::reset();
}

void RuntimeBridge::beginFrame() {
  const double frame_dt = updateRuntimeFrameState();
  RuntimeInputSnapshot::poll();
  MouseLookController::updateFromInputSnapshot();
  KeyboardMovementController::updateFromInputSnapshot(frame_dt);
  GamepadController::updateFromInputSnapshot(frame_dt);
  Gl33RuntimeState::beginFrame(
      RuntimeFrameState::framebufferWidth(), RuntimeFrameState::framebufferHeight());
}

void RuntimeBridge::endFrame() {
  auto* window = GlfwRuntimeWindow::window();
  if (window == nullptr) {
    return;
  }

  render::gl33::Gl33HudRenderer::render();
  FrameTitleController::update();
  glfwSwapBuffers(window);
  glfwPollEvents();
}

bool RuntimeBridge::shouldCloseRuntime() {
  return GlfwRuntimeWindow::shouldClose();
}

void RuntimeBridge::shutdownRuntime() {
  FrameTitleController::reset();
  clearFrameClock();
  render::gl33::Gl33HudRenderer::reset();
  GlfwRuntimeWindow::shutdown();
}

int RuntimeBridge::currentWorld() {
  return hg_runtime_world();
}

void RuntimeBridge::switchWorld(int world_id) {
  hg_runtime_set_world(world_id);
}

double RuntimeBridge::cameraX() {
  return hg_runtime_camera_x();
}

double RuntimeBridge::cameraY() {
  return hg_runtime_camera_y();
}

double RuntimeBridge::cameraZ() {
  return hg_runtime_camera_z();
}

double RuntimeBridge::cameraFrontX() {
  return hg_runtime_camera_front_x();
}

double RuntimeBridge::cameraFrontY() {
  return hg_runtime_camera_front_y();
}

double RuntimeBridge::cameraFrontZ() {
  return hg_runtime_camera_front_z();
}

double RuntimeBridge::cameraUpX() {
  return hg_runtime_camera_up_x();
}

double RuntimeBridge::cameraUpY() {
  return hg_runtime_camera_up_y();
}

double RuntimeBridge::cameraUpZ() {
  return hg_runtime_camera_up_z();
}

void RuntimeBridge::setCameraPosition(double x, double y, double z) {
  hg_runtime_set_camera_position(x, y, z);
}

bool RuntimeBridge::isSpacePressed() {
  return RuntimeInputSnapshot::keyState(GLFW_KEY_SPACE) == GLFW_PRESS;
}

void RuntimeBridge::setCppVerticalMotionWorld(int world_id, bool enabled) {
  hg_runtime_set_cpp_vertical_motion(world_id, enabled ? 1 : 0);
}

bool RuntimeBridge::isCppVerticalMotionWorld(int world_id) {
  return hg_runtime_is_cpp_vertical_motion_enabled(world_id) != 0;
}

void RuntimeBridge::setWorldGravityPhysicsEnabled(int world_id, bool enabled) {
  setCppVerticalMotionWorld(world_id, enabled);
}

bool RuntimeBridge::isWorldGravityPhysicsEnabled(int world_id) {
  return isCppVerticalMotionWorld(world_id);
}

}  // namespace hg
