#include "render/gl33/Gl33VertexArray.hpp"

namespace hg::render::gl33 {

Gl33VertexArray::Gl33VertexArray() {
  api().GenVertexArrays(1, &id_);
}

Gl33VertexArray::~Gl33VertexArray() {
  release();
}

Gl33VertexArray::Gl33VertexArray(Gl33VertexArray&& other) noexcept : id_(other.id_) {
  other.id_ = 0;
}

Gl33VertexArray& Gl33VertexArray::operator=(Gl33VertexArray&& other) noexcept {
  if (this != &other) {
    release();
    id_ = other.id_;
    other.id_ = 0;
  }
  return *this;
}

void Gl33VertexArray::bind() const {
  api().BindVertexArray(id_);
}

void Gl33VertexArray::release() {
  if (id_ != 0) {
    api().DeleteVertexArrays(1, &id_);
    id_ = 0;
  }
}

}  // namespace hg::render::gl33
