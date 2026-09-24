// Manual GPU check: run from the repository or a build with datasets/ present.
#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33WorldRenderer.hpp"
#include "world/PortalController.hpp"
#include "world/World11Landmarks.hpp"
#include "world/World11ReefFishSchool.hpp"
#include "world/World11WaterSurface.hpp"
#include "world/CppWorldGl33.hpp"
#include "hg_runtime_bridge.hpp"
#include "hg_runtime_api.h"
#include "hg_engine.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace hg::render::gl33;
using Clock = std::chrono::steady_clock;

void saveBitmap(const std::string& path, int width, int height) {
  const int stride = (width * 3 + 3) & ~3;
  std::vector<unsigned char> pixels(stride * height);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0, width, height, 0x80E0, GL_UNSIGNED_BYTE, pixels.data());
  std::ofstream out(path, std::ios::binary);
  auto word = [&out](std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.put(static_cast<char>(value >> (8 * i)));
  };
  out.write("BM", 2); word(54 + pixels.size(), 4); word(0, 4); word(54, 4);
  word(40, 4); word(width, 4); word(height, 4); word(1, 2); word(24, 2);
  word(0, 4); word(pixels.size(), 4);
  for (int i = 0; i < 4; ++i) word(0, 4);
  out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
  if (!out) throw std::runtime_error("Could not write bitmap");
}

int main(int argc, char** argv) {
  GLFWwindow* window = nullptr;
  try {
    if (argc != 2) throw std::runtime_error("Usage: world11_scene_probe OUTPUT_DIRECTORY");
    std::string output(argv[1]);

    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(1280, 800, "World 11 scene probe", nullptr, nullptr);
    if (!window) throw std::runtime_error("OpenGL context unavailable");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    if (!api().load()) throw std::runtime_error(api().loadError());
    using GenQueries = void (APIENTRY*)(int, unsigned*);
    using BeginQuery = void (APIENTRY*)(unsigned, unsigned);
    using EndQuery = void (APIENTRY*)(unsigned);
    using GetQuery = void (APIENTRY*)(unsigned, unsigned, std::uint64_t*);
    using DeleteQueries = void (APIENTRY*)(int, const unsigned*);
    auto gen = reinterpret_cast<GenQueries>(glfwGetProcAddress("glGenQueries"));
    auto begin = reinterpret_cast<BeginQuery>(glfwGetProcAddress("glBeginQuery"));
    auto end = reinterpret_cast<EndQuery>(glfwGetProcAddress("glEndQuery"));
    auto get = reinterpret_cast<GetQuery>(glfwGetProcAddress("glGetQueryObjectui64v"));
    auto remove = reinterpret_cast<DeleteQueries>(glfwGetProcAddress("glDeleteQueries"));
    if (!gen || !begin || !end || !get || !remove) throw std::runtime_error("GPU timers unavailable");
    unsigned query = 0;
    gen(1, &query);
    std::cout << "GPU: " << glGetString(GL_RENDERER) << "\n1280x800, no MSAA, vsync off\n";
    std::ofstream csv(output + "/timings.csv");
    if (!csv) throw std::runtime_error("Output directory must exist and be writable");
    csv << "route,frame,cpu_submit_ms,gpu_ms\n";
    {
      Gl33WorldRenderer renderer(11);
      const hg::world::Portal portal{0, hg::world::kWorld11ReturnPortalY, -20, 1.5f, 6, 0, 0.3f, -45};
      Gl33WorldFrame frame{11, {{0, 3.5f, -30}, {0, 0, -1}, {0, 1, 0}, 1280, 800}, 0, 0, &portal, 1};
      glViewport(0, 0, 1280, 800);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LEQUAL);
      for (int route = 0; route < 3; ++route) {
        std::vector<double> cpu, gpu;
        for (int i = 0; i < 150; ++i) {
          frame.time = (route * 150 + i) / 60.0f;
          frame.camera.position[0] = route == 0 ? 0.0f : (route == 1 ? i * 0.025f : i * 0.8f);
          begin(0x88BF, query); // GL_TIME_ELAPSED
          auto start = Clock::now();
          renderer.render(frame);
          auto stop = Clock::now();
          end(0x88BF);
          std::uint64_t nanoseconds = 0;
          get(query, 0x8866, &nanoseconds); // GL_QUERY_RESULT
          double cpuMs = std::chrono::duration<double, std::milli>(stop - start).count();
          double gpuMs = nanoseconds / 1.0e6;
          csv << route << ',' << i << ',' << cpuMs << ',' << gpuMs << '\n';
          if (i >= 30) { cpu.push_back(cpuMs); gpu.push_back(gpuMs); }
          if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error during route");
        }
        std::sort(cpu.begin(), cpu.end()); std::sort(gpu.begin(), gpu.end());
        std::cout << "route=" << route << " cpu median/p95=" << cpu[60] << '/' << cpu[114]
                  << " gpu median/p95=" << gpu[60] << '/' << gpu[114] << " ms\n";
      }
      struct View { const char* name; std::array<float, 3> eye, target; };
      const View views[] = {
          {"entry", {0, 3.5f, -30}, {0, 2, -65}},
          {"arch", {-14, 1, -43}, {-16, -3, -64}},
          {"seep", {-27, 4, -48}, {-35, -3, -62}},
          {"colony", {25, 3, -63}, {34, -1, -82}},
          {"surface", {0, 11, -45}, {0, 17, -60}},
          {"return", {0, 2, -38}, {0, 0, -20}},
      };
      for (const auto& view : views) {
        float length = 0;
        for (int axis = 0; axis < 3; ++axis) {
          frame.camera.position[axis] = view.eye[axis];
          frame.camera.front[axis] = view.target[axis] - view.eye[axis];
          length += frame.camera.front[axis] * frame.camera.front[axis];
        }
        for (float& value : frame.camera.front) value /= std::sqrt(length);
        frame.time = 8.0f;
        renderer.render(frame);
        saveBitmap(output + "/" + view.name + ".bmp", 1280, 800);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error capturing view");
      }
      // Close-up of the first reef fish from its side, using its own trajectory.
      {
        auto trajectory = hg::world::world11ReefFishTrajectory(0);
        const auto fish = trajectory.sample(8.0);
        const float speed = std::sqrt(fish.velocity.x * fish.velocity.x +
                                      fish.velocity.z * fish.velocity.z);
        const float sideX = speed > 1.0e-4f ? -fish.velocity.z / speed : 1.0f;
        const float sideZ = speed > 1.0e-4f ? fish.velocity.x / speed : 0.0f;
        const float eye[3] = {fish.position.x + sideX * 1.6f,
                              fish.position.y + 0.35f,
                              fish.position.z + sideZ * 1.6f};
        float length = 0;
        for (int axis = 0; axis < 3; ++axis) {
          frame.camera.position[axis] = eye[axis];
        }
        frame.camera.front[0] = fish.position.x - eye[0];
        frame.camera.front[1] = fish.position.y - eye[1];
        frame.camera.front[2] = fish.position.z - eye[2];
        for (float value : frame.camera.front) length += value * value;
        for (float& value : frame.camera.front) value /= std::sqrt(length);
        frame.time = 8.0f;
        renderer.render(frame);
        saveBitmap(output + "/reef_fish.bmp", 1280, 800);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error capturing reef fish");
      }
      // Close-up of the tube sponge colony landmark.
      {
        const auto colony = hg::world::kWorld11Colony;
        const float floor = hg::world::world11SeabedHeight(colony.x, colony.z);
        const float eye[3] = {colony.x - 4.5f, floor + 2.6f, colony.z + 5.0f};
        const float target[3] = {colony.x, floor + 1.3f, colony.z};
        float length = 0;
        for (int axis = 0; axis < 3; ++axis) {
          frame.camera.position[axis] = eye[axis];
          frame.camera.front[axis] = target[axis] - eye[axis];
          length += frame.camera.front[axis] * frame.camera.front[axis];
        }
        for (float& value : frame.camera.front) value /= std::sqrt(length);
        frame.time = 8.0f;
        renderer.render(frame);
        saveBitmap(output + "/sponges.bmp", 1280, 800);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error capturing sponges");
      }
    }
    // Exercise the real world tick/collision/portal path with a current GL context.
    // No runtime input window is registered, so external keyboard input cannot interfere.
    hg::RuntimeBridge::configureRuntimeWorldCount(12);
    hg_universe_init();
    hg::RuntimeBridge::switchWorld(11);
    hg::RuntimeBridge::setWorldGravityPhysicsEnabled(11, false);
    {
      hg::Engine engine;
      hg::world::CppWorldGl33 ocean(11);
      double previousY = 0;
      float simulationTime = 0;
      for (int i = 0; i < 240; ++i) {
        hg::RuntimeBridge::setCameraPosition(0, 100, -45);
        ocean.tick(engine, 1.0 / 60.0);
        simulationTime += 1.0f / 60.0f;
        const double y = hg::RuntimeBridge::cameraY();
        const float surface = hg::world::world11WaterSurfaceHeight(0, -45, simulationTime);
        if (y >= surface - 0.1 || y < surface - 2.0 ||
            (i > 0 && std::abs(y - previousY) > 0.10)) {
          throw std::runtime_error("Camera surface clearance or continuity failed");
        }
        previousY = y;
      }
      hg::RuntimeBridge::setCameraPosition(0, -100, -45);
      ocean.tick(engine, 1.0 / 60.0);
      if (hg::RuntimeBridge::cameraY() < hg::world::world11SeabedHeight(0, -45)) {
        throw std::runtime_error("Camera penetrated seabed");
      }
      const auto arch = hg::world::kWorld11Arch;
      const float passageY = hg::world::world11SeabedHeight(arch.x, arch.z) + 3.0f;
      for (int i = 0; i <= 40; ++i) {
        const float z = arch.z + 5.0f - i * 0.25f;
        hg::RuntimeBridge::setCameraPosition(arch.x, passageY, z);
        ocean.tick(engine, 1.0 / 60.0);
        if (std::abs(hg::RuntimeBridge::cameraZ() - z) > 0.05) {
          throw std::runtime_error("Reef arch opening is blocked");
        }
      }
      hg::RuntimeBridge::setCameraPosition(0, hg::world::kWorld11ReturnPortalY, -20);
      ocean.tick(engine, 1.0 / 60.0);
      if (hg::RuntimeBridge::currentWorld() != 6) throw std::runtime_error("Return portal failed");
      hg::world::CppWorldGl33 hub(6);
      hg::RuntimeBridge::setCameraPosition(0, 0, -30);
      hub.tick(engine, 1.0 / 60.0);
      if (hg::RuntimeBridge::currentWorld() != 11 ||
          std::abs(hg::RuntimeBridge::cameraZ() + 45.0) > 0.01) {
        throw std::runtime_error("Entry portal failed");
      }
      std::cout << "Runtime checks passed: wave ceiling, seabed, arch passage, portals 11 -> 6 -> 11\n";
    }
    remove(1, &query);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
}
