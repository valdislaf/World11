#include "hg_console_renderer.hpp"
#include "hg_gl33_runtime_state.hpp"
#include "hg_runtime_bridge.hpp"
#include "hg_game_app.hpp"
#include "hg_runtime_input.hpp"
#include "world/WorldCatalog.hpp"
#include "world/WorldManager.hpp"
#include "world/WorldHost.hpp"
#include "world/WorldRegistry.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct LaunchOptions {
  int worldId = 11;
};

int parseWorld(const std::string& value) {
  std::size_t consumed = 0;
  const int worldId = std::stoi(value, &consumed);
  if (consumed != value.size() || worldId <= 0) {
    throw std::invalid_argument("invalid --world value: " + value);
  }
  return worldId;
}

LaunchOptions parseLaunchOptions(int argc, char** argv) {
  LaunchOptions options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--world") {
      if (++index >= argc) {
        throw std::invalid_argument("--world requires 11 (ocean) or 6 (return hub)");
      }
      options.worldId = parseWorld(argv[index]);
    } else {
      throw std::invalid_argument("unknown argument: " + argument);
    }
  }

  return options;
}

}  // namespace

int main(int argc, char** argv) {
  int frameCount = 0;
  double timeSeconds = 0.0;
  try {
    const LaunchOptions options = parseLaunchOptions(argc, argv);
    hg::world::WorldRegistry registry;
    hg::world::registerBuiltInWorlds(registry);
    if (!registry.hasWorld(options.worldId)) {
      throw std::invalid_argument("unknown world id: " + std::to_string(options.worldId));
    }
    hg::world::WorldManager worldManager(registry);
    hg::RuntimeBridge::configureRuntimeWorldCount(registry.maxWorldId());
    for (const int id : registry.worldIds()) {
      hg::RuntimeBridge::setWorldGravityPhysicsEnabled(
          id, registry.definition(id).gravity_physics_enabled);
    }
    hg::RuntimeBridge::initRuntime();

    hg::Gl33RuntimeState::printDiagnostics(
        "core-default", registry.definition(options.worldId).name.c_str());

    {
      auto input = std::make_unique<hg::RuntimeInput>();
      auto world = std::make_unique<hg::world::WorldHost>(worldManager, options.worldId);
      auto renderer = std::make_unique<hg::ConsoleRenderer>();
      hg::GameApp app(std::move(input), std::move(world), std::move(renderer));
      app.runUntilQuit(1.0 / 60.0);
      frameCount = app.engine().frameCount();
      timeSeconds = app.engine().timeSeconds();
    }
    hg::RuntimeBridge::shutdownRuntime();
  } catch (const std::exception& error) {
    hg::RuntimeBridge::shutdownRuntime();
    std::cerr << "Startup failed: " << error.what() << '\n';
    return 1;
  }

  std::cout << "Frame count: " << frameCount << "\n";
  std::cout << "Time seconds: " << timeSeconds << "\n";
  return 0;
}
