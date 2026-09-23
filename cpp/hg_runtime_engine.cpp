#include <cstdint>
#include <new>

namespace {

struct HgEngineHandle {
  int frame_count = 0;
  double time_seconds = 0.0;
};

}  // namespace

extern "C" void* hg_engine_create() {
  return new (std::nothrow) HgEngineHandle{};
}

extern "C" void hg_engine_destroy(void* engine) {
  delete static_cast<HgEngineHandle*>(engine);
}

extern "C" void hg_engine_update(void* engine, double dt) {
  auto* handle = static_cast<HgEngineHandle*>(engine);
  if (handle == nullptr) {
    return;
  }
  ++handle->frame_count;
  handle->time_seconds += dt;
}

extern "C" int hg_engine_frame_count(void* engine) {
  const auto* handle = static_cast<const HgEngineHandle*>(engine);
  return handle != nullptr ? handle->frame_count : 0;
}

extern "C" double hg_engine_time_seconds(void* engine) {
  const auto* handle = static_cast<const HgEngineHandle*>(engine);
  return handle != nullptr ? handle->time_seconds : 0.0;
}
