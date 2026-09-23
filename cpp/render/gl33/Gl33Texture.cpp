#include "render/gl33/Gl33Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hg::render::gl33 {

namespace {

using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;

std::vector<unsigned char> decryptFget(const std::string& path) {
  std::string extension;
  const std::size_t dot = path.find_last_of('.');
  if (dot != std::string::npos) {
    extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                     return static_cast<char>(std::tolower(value));
                   });
  }
  if (extension != ".fget") {
    throw std::runtime_error("Gl33Texture accepts only .fget assets: " + path);
  }

  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    throw std::runtime_error("Cannot open FGET texture: " + path);
  }
  const std::streamoff fileSize = stream.tellg();
  if (fileSize <= 5 ||
      fileSize > static_cast<std::streamoff>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("Invalid FGET texture size: " + path);
  }
  stream.seekg(0, std::ios::beg);
  std::vector<unsigned char> packed(static_cast<std::size_t>(fileSize));
  if (!stream.read(reinterpret_cast<char*>(packed.data()), fileSize)) {
    throw std::runtime_error("Cannot read FGET texture: " + path);
  }
  if (std::memcmp(packed.data(), "FGET", 4) != 0) {
    throw std::runtime_error("Invalid FGET magic: " + path);
  }

  const unsigned char key = packed[4];
  std::vector<unsigned char> decrypted(packed.begin() + 5, packed.end());
  for (unsigned char& value : decrypted) {
    value ^= key;
  }
  return decrypted;
}

Pixels decodeFgetPng(const std::string& path, int& width, int& height) {
  const std::vector<unsigned char> decrypted = decryptFget(path);
  int channels = 0;
  stbi_set_flip_vertically_on_load(0);
  Pixels pixels(stbi_load_from_memory(
      decrypted.data(), static_cast<int>(decrypted.size()),
      &width, &height, &channels, STBI_rgb_alpha), &stbi_image_free);
  if (!pixels || width <= 0 || height <= 0) {
    width = 0;
    height = 0;
    const char* reason = stbi_failure_reason();
    throw std::runtime_error("PNG decode failed for " + path + ": " +
                             (reason != nullptr ? reason : "unknown error"));
  }
  return pixels;
}

bool isWorld11FishColorAsset(const std::string& path) {
  constexpr const char* kFishAsset = "datasets/0x00000019.fget";
  return path == kFishAsset;
}

}  // namespace

Gl33Texture::~Gl33Texture() {
  release();
}

Gl33Texture::Gl33Texture(Gl33Texture&& other) noexcept
    : texture_(other.texture_),
      width_(other.width_),
      height_(other.height_),
      rgbaPixels_(std::move(other.rgbaPixels_)) {
  other.texture_ = 0;
  other.width_ = 0;
  other.height_ = 0;
}

Gl33Texture& Gl33Texture::operator=(Gl33Texture&& other) noexcept {
  if (this != &other) {
    release();
    texture_ = other.texture_;
    width_ = other.width_;
    height_ = other.height_;
    rgbaPixels_ = std::move(other.rgbaPixels_);
    other.texture_ = 0;
    other.width_ = 0;
    other.height_ = 0;
  }
  return *this;
}

void Gl33Texture::loadFget(const std::string& path, Gl33TextureWrap wrap) {
  int decodedWidth = 0;
  int decodedHeight = 0;
  Pixels pixels = decodeFgetPng(path, decodedWidth, decodedHeight);

  width_ = decodedWidth;
  height_ = decodedHeight;
  const std::size_t pixelByteCount =
      static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4U;
  rgbaPixels_.assign(pixels.get(), pixels.get() + pixelByteCount);

  release();
  Gl33Api& gl = api();
  gl.GenTextures(1, &texture_);
  gl.BindTexture(kTexture2D, texture_);
  gl.PixelStorei(kUnpackAlignment, 1);
  gl.TexParameteri(
      kTexture2D, kTextureMinFilter, static_cast<Int>(kLinearMipmapLinear));
  gl.TexParameteri(kTexture2D, kTextureMagFilter, static_cast<Int>(kLinear));
  const Int wrapValue = static_cast<Int>(wrap == Gl33TextureWrap::Repeat
      ? kRepeat : kClampToEdge);
  gl.TexParameteri(kTexture2D, kTextureWrapS, wrapValue);
  gl.TexParameteri(kTexture2D, kTextureWrapT, wrapValue);
  gl.TexImage2D(kTexture2D, 0, kRgba8, width_, height_, 0,
                kRgba, kUnsignedByte, pixels.get());
  gl.GenerateMipmap(kTexture2D);
  gl.BindTexture(kTexture2D, 0);

  // The GPU texture remains the colored fish (0x19). For procedural body
  // generation alphaAt() exposes the artist-authored body-only mask (0x1A),
  // excluding fins and tail rays from the volumetric shell.
  if (isWorld11FishColorAsset(path)) {
    constexpr const char* kBodyMaskAsset = "datasets/0x0000001A.fget";
    int maskWidth = 0;
    int maskHeight = 0;
    Pixels maskPixels = decodeFgetPng(kBodyMaskAsset, maskWidth, maskHeight);
    if (maskWidth != width_ || maskHeight != height_) {
      throw std::runtime_error(
          "World11 fish body mask dimensions do not match color texture");
    }
    rgbaPixels_.assign(maskPixels.get(), maskPixels.get() + pixelByteCount);

    // Extend only the trailing edge of the CPU body mask slightly toward the
    // tail. This creates a small overlap between the volumetric body and the
    // flat tail cutout, preventing a visible crack while leaving the source
    // mask and the GPU color texture unchanged.
    constexpr int kTailOverlapPixels = 18;
    const std::vector<unsigned char> sourceMask = rgbaPixels_;
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        unsigned char maximumAlpha = 0;
        const int sourceBegin = std::max(0, x - kTailOverlapPixels);
        for (int sourceX = sourceBegin; sourceX <= x; ++sourceX) {
          const std::size_t sourceIndex =
              (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(sourceX)) * 4U + 3U;
          maximumAlpha = std::max(maximumAlpha, sourceMask[sourceIndex]);
        }
        const std::size_t destinationIndex =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
             static_cast<std::size_t>(x)) * 4U + 3U;
        rgbaPixels_[destinationIndex] = maximumAlpha;
      }
    }
  }
}

void Gl33Texture::bind(UInt unit) const {
  api().ActiveTexture(kTexture0 + unit);
  api().BindTexture(kTexture2D, texture_);
}

int Gl33Texture::width() const {
  return width_;
}

int Gl33Texture::height() const {
  return height_;
}

float Gl33Texture::alphaAt(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_ || rgbaPixels_.empty()) {
    return 0.0f;
  }
  const std::size_t index =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
       static_cast<std::size_t>(x)) * 4U + 3U;
  return static_cast<float>(rgbaPixels_[index]) / 255.0f;
}

void Gl33Texture::release() {
  if (texture_ != 0) {
    api().DeleteTextures(1, &texture_);
    texture_ = 0;
  }
}

}  // namespace hg::render::gl33
