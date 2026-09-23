#pragma once

#include "render/gl33/Gl33Api.hpp"

#include <string>
#include <vector>

namespace hg::render::gl33 {

enum class Gl33TextureWrap {
  ClampToEdge,
  Repeat,
};

/// <summary>Move-only OpenGL 3.3 texture loaded exclusively from an encrypted FGET asset.</summary>
class Gl33Texture final {
public:
  Gl33Texture() = default;
  ~Gl33Texture();
  Gl33Texture(const Gl33Texture&) = delete;
  Gl33Texture& operator=(const Gl33Texture&) = delete;
  Gl33Texture(Gl33Texture&& other) noexcept;
  Gl33Texture& operator=(Gl33Texture&& other) noexcept;

  /// <summary>Decrypts FGET bytes, decodes the contained PNG in memory and uploads RGBA8.</summary>
  void loadFget(const std::string& path,
                Gl33TextureWrap wrap = Gl33TextureWrap::ClampToEdge);
  void bind(UInt unit = 0) const;
  int width() const;
  int height() const;

  /// <summary>Returns normalized source alpha at an integer PNG texel.</summary>
  float alphaAt(int x, int y) const;

private:
  void release();

  UInt texture_ = 0;
  int width_ = 0;
  int height_ = 0;
  std::vector<unsigned char> rgbaPixels_;
};

}  // namespace hg::render::gl33
