#include "render/gl33/Gl33ShaderProgram.hpp"

#include "render/gl33/Gl33Texture.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr char kWorld11SeabedShaderMarker[] =
    "float seabedHeight(vec2 point)";
constexpr char kWorld11SeabedTexturePath[] =
    "datasets/0x0000001B.fget";
constexpr float kWorld11SeabedUvScale = 0.085f;

bool isWorld11SeabedShader(const char* vertexSource) {
  return vertexSource != nullptr &&
      std::strstr(vertexSource, kWorld11SeabedShaderMarker) != nullptr;
}

std::string world11SeabedFragmentSource(const char* fragmentSource) {
  if (fragmentSource == nullptr) {
    throw std::runtime_error("World 11 seabed fragment shader is null");
  }

  std::string source{fragmentSource};
  constexpr char kUniformAnchor[] = "out vec4 fragmentColor;";
  constexpr char kLightingAnchor[] =
      "  vec3 litColor = uBaseColor.rgb * light;";
  constexpr char kTextureUniforms[] =
      "uniform sampler2D uSandTexture;\n"
      "uniform float uSandUvScale;\n\n";
  constexpr char kTexturedLighting[] =
      "  vec2 sandUv = vWorldPosition.xz * uSandUvScale;\n"
      "  vec3 sandAlbedo = texture(uSandTexture, sandUv).rgb;\n"
      "  vec3 sandTint = mix(vec3(1.0),\n"
      "      clamp(uBaseColor.rgb * 1.8, 0.0, 1.0), 0.18);\n"
      "  vec3 litColor = sandAlbedo * sandTint * light;";

  const std::size_t uniformPosition = source.find(kUniformAnchor);
  const std::size_t lightingPosition = source.find(kLightingAnchor);
  if (uniformPosition == std::string::npos ||
      lightingPosition == std::string::npos) {
    throw std::runtime_error(
        "World 11 seabed fragment shader layout has changed");
  }

  source.insert(uniformPosition, kTextureUniforms);
  const std::size_t adjustedLightingPosition = source.find(kLightingAnchor);
  source.replace(adjustedLightingPosition,
                 std::strlen(kLightingAnchor), kTexturedLighting);
  return source;
}

}  // namespace

Gl33ShaderProgram::~Gl33ShaderProgram() {
  release();
}

Gl33ShaderProgram::Gl33ShaderProgram(Gl33ShaderProgram&& other) noexcept
    : program_(other.program_),
      isWorld11SeabedProgram_(other.isWorld11SeabedProgram_),
      world11SeabedTexture_(std::move(other.world11SeabedTexture_)) {
  other.program_ = 0;
  other.isWorld11SeabedProgram_ = false;
}

Gl33ShaderProgram& Gl33ShaderProgram::operator=(Gl33ShaderProgram&& other) noexcept {
  if (this != &other) {
    release();
    program_ = other.program_;
    isWorld11SeabedProgram_ = other.isWorld11SeabedProgram_;
    world11SeabedTexture_ = std::move(other.world11SeabedTexture_);
    other.program_ = 0;
    other.isWorld11SeabedProgram_ = false;
  }
  return *this;
}

void Gl33ShaderProgram::build(const char* vertexSource, const char* fragmentSource) {
  release();
  const bool isWorld11Seabed = isWorld11SeabedShader(vertexSource);
  std::string fragmentSourceStorage;
  const char* compiledFragmentSource = fragmentSource;
  if (isWorld11Seabed) {
    fragmentSourceStorage = world11SeabedFragmentSource(fragmentSource);
    compiledFragmentSource = fragmentSourceStorage.c_str();
  }

  const UInt vertexShader = compile(kVertexShader, vertexSource);
  UInt fragmentShader = 0;
  try {
    fragmentShader = compile(kFragmentShader, compiledFragmentSource);
    program_ = api().CreateProgram();
    api().AttachShader(program_, vertexShader);
    api().AttachShader(program_, fragmentShader);
    api().LinkProgram(program_);

    Int linked = 0;
    api().GetProgramiv(program_, kLinkStatus, &linked);
    if (linked == 0) {
      throw std::runtime_error("GLSL program link failed:\n" + programLog(program_));
    }
    if (isWorld11Seabed) {
      auto texture = std::make_unique<Gl33Texture>();
      texture->loadFget(kWorld11SeabedTexturePath, Gl33TextureWrap::Repeat);
      world11SeabedTexture_ = std::move(texture);
      isWorld11SeabedProgram_ = true;
    }

    api().DetachShader(program_, vertexShader);
    api().DetachShader(program_, fragmentShader);
    api().DeleteShader(vertexShader);
    api().DeleteShader(fragmentShader);
  } catch (...) {
    api().DeleteShader(vertexShader);
    if (fragmentShader != 0) {
      api().DeleteShader(fragmentShader);
    }
    release();
    throw;
  }
}

void Gl33ShaderProgram::use() const {
  api().UseProgram(program_);
  if (isWorld11SeabedProgram_ && world11SeabedTexture_ != nullptr) {
    world11SeabedTexture_->bind(0);
    setInt("uSandTexture", 0);
    setFloat("uSandUvScale", kWorld11SeabedUvScale);
  }
}

void Gl33ShaderProgram::setMatrix4(const char* name, const float* value) const {
  const Int location = api().GetUniformLocation(program_, name);
  if (location >= 0) {
    api().UniformMatrix4fv(location, 1, kFalse, value);
  }
}

void Gl33ShaderProgram::setFloat(const char* name, float value) const {
  const Int location = api().GetUniformLocation(program_, name);
  if (location >= 0) {
    api().Uniform1f(location, value);
  }
}

void Gl33ShaderProgram::setInt(const char* name, int value) const {
  const Int location = api().GetUniformLocation(program_, name);
  if (location >= 0) {
    api().Uniform1i(location, value);
  }
}

void Gl33ShaderProgram::setVec3(const char* name, float x, float y, float z) const {
  const Int location = api().GetUniformLocation(program_, name);
  if (location >= 0) {
    api().Uniform3f(location, x, y, z);
  }
}

void Gl33ShaderProgram::setVec4(const char* name, float x, float y, float z, float w) const {
  const Int location = api().GetUniformLocation(program_, name);
  if (location >= 0) {
    api().Uniform4f(location, x, y, z, w);
  }
}

UInt Gl33ShaderProgram::compile(Enum shaderType, const char* source) {
  const UInt shader = api().CreateShader(shaderType);
  api().ShaderSource(shader, 1, &source, nullptr);
  api().CompileShader(shader);
  Int compiled = 0;
  api().GetShaderiv(shader, kCompileStatus, &compiled);
  if (compiled == 0) {
    const std::string log = shaderLog(shader);
    api().DeleteShader(shader);
    throw std::runtime_error("GLSL shader compile failed:\n" + log);
  }
  return shader;
}

std::string Gl33ShaderProgram::shaderLog(UInt shader) {
  Int length = 0;
  api().GetShaderiv(shader, kInfoLogLength, &length);
  std::vector<Char> buffer(static_cast<std::size_t>(std::max(length, 1)), '\0');
  api().GetShaderInfoLog(shader, static_cast<SizeI>(buffer.size()), nullptr, buffer.data());
  return std::string(buffer.data());
}

std::string Gl33ShaderProgram::programLog(UInt program) {
  Int length = 0;
  api().GetProgramiv(program, kInfoLogLength, &length);
  std::vector<Char> buffer(static_cast<std::size_t>(std::max(length, 1)), '\0');
  api().GetProgramInfoLog(program, static_cast<SizeI>(buffer.size()), nullptr, buffer.data());
  return std::string(buffer.data());
}

void Gl33ShaderProgram::release() {
  world11SeabedTexture_.reset();
  isWorld11SeabedProgram_ = false;
  if (program_ != 0) {
    api().DeleteProgram(program_);
    program_ = 0;
  }
}

}  // namespace hg::render::gl33
