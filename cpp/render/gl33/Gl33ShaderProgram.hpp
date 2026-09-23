#pragma once

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Texture.hpp"

#include <memory>
#include <string>

namespace hg::render::gl33 {

/// <summary>Move-only GLSL 330 Core shader program.</summary>
class Gl33ShaderProgram final {
public:
  Gl33ShaderProgram() = default;
  ~Gl33ShaderProgram();
  Gl33ShaderProgram(const Gl33ShaderProgram&) = delete;
  Gl33ShaderProgram& operator=(const Gl33ShaderProgram&) = delete;
  Gl33ShaderProgram(Gl33ShaderProgram&& other) noexcept;
  Gl33ShaderProgram& operator=(Gl33ShaderProgram&& other) noexcept;

  void build(const char* vertexSource, const char* fragmentSource);
  void use() const;
  void setMatrix4(const char* name, const float* value) const;
  void setFloat(const char* name, float value) const;
  void setInt(const char* name, int value) const;
  void setVec3(const char* name, float x, float y, float z) const;
  void setVec4(const char* name, float x, float y, float z, float w) const;

private:
  static UInt compile(Enum shaderType, const char* source);
  static std::string shaderLog(UInt shader);
  static std::string programLog(UInt program);
  void release();

  UInt program_ = 0;
  bool isWorld11SeabedProgram_ = false;
  std::unique_ptr<Gl33Texture> world11SeabedTexture_;
};

}  // namespace hg::render::gl33
