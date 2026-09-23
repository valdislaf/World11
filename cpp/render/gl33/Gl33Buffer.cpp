#include "render/gl33/Gl33Buffer.hpp"

#include <utility>

namespace hg::render::gl33 {

Gl33Buffer::Gl33Buffer(Enum target) : target_(target) {
  api().GenBuffers(1, &id_);
}

Gl33Buffer::~Gl33Buffer() {
  release();
}

Gl33Buffer::Gl33Buffer(Gl33Buffer&& other) noexcept
    : target_(other.target_), id_(other.id_) {
  other.target_ = 0;
  other.id_ = 0;
}

Gl33Buffer& Gl33Buffer::operator=(Gl33Buffer&& other) noexcept {
  if (this != &other) {
    release();
    target_ = other.target_;
    id_ = other.id_;
    other.target_ = 0;
    other.id_ = 0;
  }
  return *this;
}

void Gl33Buffer::bind() const {
  api().BindBuffer(target_, id_);
}

void Gl33Buffer::upload(const void* data, SizePtr byteCount, Enum usage) const {
  bind();
  api().BufferData(target_, byteCount, data, usage);
}

Enum Gl33Buffer::target() const {
  return target_;
}

void Gl33Buffer::release() {
  if (id_ != 0) {
    api().DeleteBuffers(1, &id_);
    id_ = 0;
  }
}

}  // namespace hg::render::gl33
