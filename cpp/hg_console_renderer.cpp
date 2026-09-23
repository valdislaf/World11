#include "hg_console_renderer.hpp"

#include "hg_engine.hpp"
#include "hg_runtime_bridge.hpp"

#include <iostream>

namespace hg {

void ConsoleRenderer::render(const Engine& engine, const IWorld&) {
  static int last_world = -1;
  const int world = RuntimeBridge::currentWorld();

  if (engine.frameCount() == 1) {
    std::cout << "CPP wrapper is running.\n";
  }

  if (world != last_world) {
    std::cout << "World: " << world << " | Frame: " << engine.frameCount() << "\n";
    last_world = world;
  }
}

}  // namespace hg
