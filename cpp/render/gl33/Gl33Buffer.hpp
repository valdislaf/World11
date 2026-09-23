#pragma once

#include "render/gl33/Gl33Api.hpp"

namespace hg::render::gl33 {

/// <summary>Move-only OpenGL 3.3 buffer owner.</summary>
class Gl33Buffer final {
public:
  explicit Gl33Buffer(Enum target);
  ~Gl33Buffer();
  Gl33Buffer(const Gl33Buffer&) = delete;
  Gl33Buffer& operator=(const Gl33Buffer&) = delete;
  Gl33Buffer(Gl33Buffer&& other) noexcept;
  Gl33Buffer& operator=(Gl33Buffer&& other) noexcept;

  void bind() const;
  void upload(const void* data, SizePtr byteCount, Enum usage = kStaticDraw) const;
  Enum target() const;

private:
  void release();

  Enum target_ = 0;
  UInt id_ = 0;
};

}  // namespace hg::render::gl33
