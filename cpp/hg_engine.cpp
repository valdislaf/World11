#include "hg_engine.hpp"
#include "hg_runtime_api.h"

#include <stdexcept>

namespace hg {

Engine::Engine() : handle_(hg_engine_create()) {
  if (handle_ == nullptr) {
    throw std::runtime_error("hg_engine_create returned null");
  }
}

Engine::~Engine() {
  if (handle_ != nullptr) {
    hg_engine_destroy(handle_);
    handle_ = nullptr;
  }
}

Engine::Engine(Engine&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

Engine& Engine::operator=(Engine&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (handle_ != nullptr) {
    hg_engine_destroy(handle_);
  }

  handle_ = other.handle_;
  other.handle_ = nullptr;
  return *this;
}

void Engine::update(double dt) {
  hg_engine_update(handle_, dt);
}

int Engine::frameCount() const {
  return hg_engine_frame_count(handle_);
}

double Engine::timeSeconds() const {
  return hg_engine_time_seconds(handle_);
}

}  // namespace hg
