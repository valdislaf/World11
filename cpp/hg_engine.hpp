#pragma once

namespace hg {

/// <summary>RAII wrapper around runtime engine counters.</summary>
class Engine {
public:
  /// <summary>Creates engine handle in C++ runtime.</summary>
  Engine();
  /// <summary>Destroys engine handle in C++ runtime.</summary>
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  Engine(Engine&& other) noexcept;
  Engine& operator=(Engine&& other) noexcept;

  /// <summary>Advances engine counters by delta time.</summary>
  /// <param name="dt">Frame delta time in seconds.</param>
  void update(double dt);
  /// <returns>Number of processed frames.</returns>
  int frameCount() const;
  /// <returns>Accumulated simulated time in seconds.</returns>
  double timeSeconds() const;

private:
  void* handle_;
};

}  // namespace hg
