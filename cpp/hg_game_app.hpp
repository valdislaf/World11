#pragma once

#include "hg_engine.hpp"
#include "hg_interfaces.hpp"

#include <memory>

namespace hg {

/// <summary>Main application loop coordinator (input, world, renderer).</summary>
class GameApp {
public:
  /// <summary>Constructs the app with required runtime components.</summary>
  /// <param name="input">Input source; ownership transfers to this <c>GameApp</c>. Must not be <c>nullptr</c>.</param>
  /// <param name="world">Initial active world; ownership transfers to this <c>GameApp</c>. Must not be <c>nullptr</c>.</param>
  /// <param name="renderer">Renderer invoked after each world tick; ownership transfers to this <c>GameApp</c>. Must not be <c>nullptr</c>.</param>
  GameApp(std::unique_ptr<IInput> input,
          std::unique_ptr<IWorld> world,
          std::unique_ptr<IRenderer> renderer);

  /// <summary>Runs frames until input reports quit.</summary>
  /// <param name="dt">Delta time passed into world tick.</param>
  void runUntilQuit(double dt);
  /// <returns>Engine facade with frame/time counters.</returns>
  const Engine& engine() const;

private:
  Engine engine_;
  std::unique_ptr<IInput> input_;
  std::unique_ptr<IWorld> world_;
  std::unique_ptr<IRenderer> renderer_;
};

}  // namespace hg
