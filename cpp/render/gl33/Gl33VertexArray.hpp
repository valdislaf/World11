#pragma once

#include "render/gl33/Gl33Api.hpp"

namespace hg::render::gl33 {

/// <summary>Move-only OpenGL 3.3 vertex-array owner.</summary>
class Gl33VertexArray final {
public:
  Gl33VertexArray();
  ~Gl33VertexArray();
  Gl33VertexArray(const Gl33VertexArray&) = delete;
  Gl33VertexArray& operator=(const Gl33VertexArray&) = delete;
  Gl33VertexArray(Gl33VertexArray&& other) noexcept;
  Gl33VertexArray& operator=(Gl33VertexArray&& other) noexcept;

  void bind() const;

private:
  void release();
  UInt id_ = 0;
};

}  // namespace hg::render::gl33
