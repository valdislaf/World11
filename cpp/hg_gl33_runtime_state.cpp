#include "hg_gl33_runtime_state.hpp"

#include "hg_glfw_runtime_window.hpp"
#include "render/gl33/Gl33Api.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace hg {

namespace {

const char* stringOrUnavailable(const render::gl33::UByte* value) {
  return value != nullptr ? reinterpret_cast<const char*>(value) : "unavailable";
}

}  // namespace

void Gl33RuntimeState::initialize() {
  using namespace render::gl33;
  Gl33Api& gl = api();
  if (!gl.load()) {
    throw std::runtime_error(gl.loadError());
  }

  GLFWwindow* window = GlfwRuntimeWindow::window();
  if (window == nullptr) {
    throw std::runtime_error("OpenGL 3.3 initialization requires a current GLFW window");
  }
  const int major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
  const int minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
  const int profile = glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE);
  if ((major < 3 || (major == 3 && minor < 3)) || profile != GLFW_OPENGL_CORE_PROFILE) {
    throw std::runtime_error("Created context is not OpenGL 3.3 Core Profile");
  }

  gl.Enable(kDepthTest);
  gl.DepthFunc(kLessEqual);
  gl.Enable(kBlend);
  gl.BlendFunc(kSourceAlpha, kOneMinusSourceAlpha);
  gl.DepthMask(kTrue);
}

void Gl33RuntimeState::beginFrame(int framebufferWidth, int framebufferHeight) {
  using namespace render::gl33;
  const int width = framebufferWidth > 0 ? framebufferWidth : 1;
  const int height = framebufferHeight > 0 ? framebufferHeight : 1;
  Gl33Api& gl = api();
  gl.Viewport(0, 0, width, height);
  gl.ClearColor(0.10f, 0.32f, 0.40f, 1.0f);
  gl.Clear(kColorBufferBit | kDepthBufferBit);
}

void Gl33RuntimeState::printDiagnostics(const char* selectedRenderer,
                                        const char* selectedWorld) {
  using namespace render::gl33;
  Gl33Api& gl = api();
  GLFWwindow* window = GlfwRuntimeWindow::window();
  const int major = window != nullptr
      ? glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR) : 0;
  const int minor = window != nullptr
      ? glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR) : 0;
  const int profile = window != nullptr
      ? glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE) : 0;
  Int profileMask = 0;
  gl.GetIntegerv(kContextProfileMask, &profileMask);

  std::cout << "GL_VERSION: " << stringOrUnavailable(gl.GetString(kVersion)) << '\n';
  std::cout << "GL_RENDERER: " << stringOrUnavailable(gl.GetString(kRenderer)) << '\n';
  std::cout << "GL_VENDOR: " << stringOrUnavailable(gl.GetString(kVendor)) << '\n';
  std::cout << "GL_SHADING_LANGUAGE_VERSION: "
            << stringOrUnavailable(gl.GetString(kShadingLanguageVersion)) << '\n';
  std::cout << "GL_CONTEXT_PROFILE: "
            << ((profile == GLFW_OPENGL_CORE_PROFILE &&
                 (profileMask & kContextCoreProfileBit) != 0)
                    ? "Core" : "not Core")
            << " " << major << '.' << minor << '\n';
  std::cout << "Selected renderer: " << selectedRenderer << '\n';
  std::cout << "Selected world: " << selectedWorld << '\n';
  std::cout.flush();
}

}  // namespace hg
