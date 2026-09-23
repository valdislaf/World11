#include "hg_runtime_input.hpp"

#include "hg_runtime_bridge.hpp"

namespace hg {

void RuntimeInput::poll() {}

bool RuntimeInput::shouldQuit() const {
  return RuntimeBridge::shouldCloseRuntime();
}

}  // namespace hg
