#pragma once

namespace hg {

class Engine;

/// <summary>Input source interface for one frame of polling and quit checks.</summary>
class IInput {
public:
  virtual ~IInput() = default;
  /// <summary>Polls input state for the current frame.</summary>
  virtual void poll() = 0;
  /// <returns>True when the app loop should terminate.</returns>
  virtual bool shouldQuit() const = 0;
};

/// <summary>World runtime interface updated once per frame.</summary>
class IWorld {
public:
  virtual ~IWorld() = default;
  /// <param name="engine">Engine facade with frame/time state.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  virtual void tick(Engine& engine, double dt) = 0;
  virtual void drawScene() const {}
  virtual int getWorldId() const { return 0; }
};

/// <summary>Renderer interface called after world update.</summary>
class IRenderer {
public:
  virtual ~IRenderer() = default;
  /// <param name="engine">Engine facade with frame/time state.</param>
  /// <param name="world">Currently active world.</param>
  virtual void render(const Engine& engine, const IWorld& world) = 0;
};

}  // namespace hg
