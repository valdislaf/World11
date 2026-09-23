#pragma once

#include "hg_interfaces.hpp"

namespace hg {

/// <summary>Input adapter that maps quit state from the runtime window.</summary>
class RuntimeInput final : public IInput {
public:
  /// <summary>No-op poll, runtime polling is handled by the frame bridge.</summary>
  void poll() override;
  /// <returns>True when runtime window requests close.</returns>
  bool shouldQuit() const override;
};

}  // namespace hg
