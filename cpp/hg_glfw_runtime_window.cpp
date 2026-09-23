#include "hg_glfw_runtime_window.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

namespace hg {

namespace {

GLFWwindow* g_window = nullptr;

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

[[noreturn]] void fail(const char* message) {
  std::cerr << message << '\n';
  glfwTerminate();
  std::exit(EXIT_FAILURE);
}

}  // namespace

void GlfwRuntimeWindow::initialize() {
  if (g_window != nullptr) {
    return;
  }

  if (glfwInit() == GLFW_FALSE) {
    fail("glfwInit failed");
  }

  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_SAMPLES, 8);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  g_window = glfwCreateWindow(1520, 1000, "OpenGL 3D", nullptr, nullptr);
  if (g_window == nullptr) {
    fail("glfwCreateWindow failed: OpenGL 3.3 Core Profile is unavailable");
  }

  glfwSetWindowPos(g_window, 10, 600);
  glfwMakeContextCurrent(g_window);
  glfwSwapInterval(1);
  glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPos(g_window, 400.0, 300.0);
  glfwSetKeyCallback(g_window, keyCallback);

}

GLFWwindow* GlfwRuntimeWindow::window() {
  return g_window;
}

bool GlfwRuntimeWindow::shouldClose() {
  return g_window != nullptr && glfwWindowShouldClose(g_window) != GLFW_FALSE;
}

void GlfwRuntimeWindow::shutdown() {
  if (g_window != nullptr) {
    glfwDestroyWindow(g_window);
    g_window = nullptr;
  }
  glfwTerminate();
}

}  // namespace hg
