#pragma once

#include "hg_interfaces.hpp"

namespace hg {

/// <summary>Minimal console renderer for runtime diagnostics.</summary>
class ConsoleRenderer final : public IRenderer {
public:
  /// <summary>Prints startup and world-switch messages.</summary>
  void render(const Engine& engine, const IWorld& world) override;
};

}  // namespace hg
