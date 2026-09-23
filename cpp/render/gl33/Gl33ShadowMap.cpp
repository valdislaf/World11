#include "render/gl33/Gl33ShadowMap.hpp"

#include "render/gl33/Gl33ShaderProgram.hpp"

#include <cmath>
#include <stdexcept>

namespace hg::render::gl33 {

namespace {

constexpr int kShadowMapSize = 2048;
// Half-width of the light frustum. 80 m of seabed around the camera gives
// roughly 4 cm texels; beyond that the absorption fog hides missing shadows.
constexpr float kShadowHalfExtent = 40.0f;
// Share of the frustum placed ahead of the camera, where most visible
// receivers are.
constexpr float kShadowForwardShift = 0.55f;
// Half depth of the light frustum; covers the water surface down to the
// deepest seabed for every receiver inside the square.
constexpr float kShadowHalfDepth = 70.0f;
constexpr float kShadowCenterY = -2.0f;
constexpr float kShadowDepthBiasMeters = 0.04f;
// Underwater light is strongly scattered, so shadowed areas keep part of
// the direct term instead of dropping to ambient only.
constexpr float kShadowStrength = 0.78f;

struct Vec3 {
  float x;
  float y;
  float z;
};

using Matrix4 = std::array<float, 16>;

float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

Vec3 normalize(const Vec3& value) {
  const float length = std::sqrt(dot(value, value));
  if (length < 1.0e-6f) {
    return {0.0f, 1.0f, 0.0f};
  }
  return {value.x / length, value.y / length, value.z / length};
}

Matrix4 multiply(const Matrix4& lhs, const Matrix4& rhs) {
  Matrix4 result{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      for (int inner = 0; inner < 4; ++inner) {
        result[static_cast<std::size_t>(column * 4 + row)] +=
            lhs[static_cast<std::size_t>(inner * 4 + row)] *
            rhs[static_cast<std::size_t>(column * 4 + inner)];
      }
    }
  }
  return result;
}

/// Rotation-only light view: the frustum is positioned by the projection so
/// its origin can be snapped in light space.
Matrix4 lightRotation(const Vec3& lightDirection) {
  const Vec3 forward = normalize(lightDirection);
  const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  const Vec3 up = normalize(cross(right, forward));
  return {
      right.x, up.x, -forward.x, 0.0f,
      right.y, up.y, -forward.y, 0.0f,
      right.z, up.z, -forward.z, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
}

Matrix4 orthographic(float left, float right, float bottom, float top,
                     float nearPlane, float farPlane) {
  Matrix4 matrix{};
  matrix[0] = 2.0f / (right - left);
  matrix[5] = 2.0f / (top - bottom);
  matrix[10] = -2.0f / (farPlane - nearPlane);
  matrix[12] = -(right + left) / (right - left);
  matrix[13] = -(top + bottom) / (top - bottom);
  matrix[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
  matrix[15] = 1.0f;
  return matrix;
}

constexpr char kShadowReceiverGlsl[] = R"glsl(
uniform sampler2DShadow uShadowMap;
uniform mat4 uShadowViewProjection;
uniform int uShadowEnabled;
uniform float uShadowTexelSize;
uniform float uShadowDepthBias;
uniform float uShadowStrength;

// 1.0 = fully lit by the direct term, lower values are in shadow.
float world11ShadowVisibility(vec3 worldPosition, vec3 normal,
                              vec3 lightDirection) {
  if (uShadowEnabled == 0) {
    return 1.0;
  }
  float facing = clamp(dot(normal, normalize(-lightDirection)), 0.0, 1.0);
  // Normal offset grows at grazing angles, where depth acne is worst.
  vec3 samplePosition = worldPosition +
      normal * (0.035 + 0.11 * (1.0 - facing));
  vec4 lightClip = uShadowViewProjection * vec4(samplePosition, 1.0);
  vec3 coord = lightClip.xyz / lightClip.w * 0.5 + 0.5;
  if (coord.x <= 0.0 || coord.x >= 1.0 || coord.y <= 0.0 ||
      coord.y >= 1.0 || coord.z >= 1.0) {
    return 1.0;
  }
  float reference = coord.z - uShadowDepthBias;
  float lit = 0.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize * 1.25;
      lit += texture(uShadowMap, vec3(coord.xy + offset, reference));
    }
  }
  lit /= 9.0;
  vec2 edgeDistance = min(coord.xy, vec2(1.0) - coord.xy);
  float edgeFade = smoothstep(0.0, 0.12, min(edgeDistance.x, edgeDistance.y));
  return mix(1.0, lit, edgeFade * uShadowStrength);
}
)glsl";

}  // namespace

std::string withWorld11Shadows(const char* fragmentSource) {
  if (fragmentSource == nullptr) {
    throw std::runtime_error("World 11 shadow receiver source is null");
  }
  std::string source{fragmentSource};
  const std::size_t versionEnd = source.find('\n');
  if (source.rfind("#version", 0) != 0 || versionEnd == std::string::npos) {
    throw std::runtime_error(
        "World 11 shadow receiver must start with a #version line");
  }
  source.insert(versionEnd + 1, kShadowReceiverGlsl);
  return source;
}

void initializeWorld11ShadowReceiver(const Gl33ShaderProgram& program) {
  program.use();
  program.setInt("uShadowMap", kWorld11ShadowTextureUnit);
  program.setInt("uShadowEnabled", 0);
  api().UseProgram(0);
}

Gl33ShadowMap::Gl33ShadowMap() = default;

Gl33ShadowMap::~Gl33ShadowMap() {
  Gl33Api& gl = api();
  if (depthTexture_ != 0) {
    gl.DeleteTextures(1, &depthTexture_);
  }
  if (framebuffer_ != 0) {
    gl.DeleteFramebuffers(1, &framebuffer_);
  }
}

void Gl33ShadowMap::beginDepthPass(const Gl33Camera& camera) {
  ensureInitialized();

  const Vec3 lightDirection{kWorld11LightDirection[0],
                            kWorld11LightDirection[1],
                            kWorld11LightDirection[2]};
  const Vec3 flatFront = normalize({camera.front[0], 0.0f, camera.front[2]});
  const float forwardShift = kShadowHalfExtent * kShadowForwardShift;
  const Vec3 center{camera.position[0] + flatFront.x * forwardShift,
                    kShadowCenterY,
                    camera.position[2] + flatFront.z * forwardShift};

  view_ = lightRotation(lightDirection);
  float lightX = view_[0] * center.x + view_[4] * center.y + view_[8] * center.z;
  float lightY = view_[1] * center.x + view_[5] * center.y + view_[9] * center.z;
  const float lightZ =
      view_[2] * center.x + view_[6] * center.y + view_[10] * center.z;
  const float texelSize =
      2.0f * kShadowHalfExtent / static_cast<float>(kShadowMapSize);
  lightX = std::floor(lightX / texelSize) * texelSize;
  lightY = std::floor(lightY / texelSize) * texelSize;
  projection_ = orthographic(
      lightX - kShadowHalfExtent, lightX + kShadowHalfExtent,
      lightY - kShadowHalfExtent, lightY + kShadowHalfExtent,
      -lightZ - kShadowHalfDepth, -lightZ + kShadowHalfDepth);
  viewProjection_ = multiply(projection_, view_);
  depthBias_ = kShadowDepthBiasMeters / (2.0f * kShadowHalfDepth);

  Gl33Api& gl = api();
  gl.GetIntegerv(kViewport, previousViewport_.data());
  gl.BindFramebuffer(kFramebuffer, framebuffer_);
  gl.Viewport(0, 0, kShadowMapSize, kShadowMapSize);
  gl.Enable(kDepthTest);
  gl.DepthFunc(kLessEqual);
  gl.DepthMask(kTrue);
  gl.Disable(kBlend);
  gl.Disable(kCullFace);
  gl.Clear(kDepthBufferBit);
  gl.Enable(kPolygonOffsetFill);
  gl.PolygonOffset(1.6f, 2.0f);
}

void Gl33ShadowMap::endDepthPass() {
  Gl33Api& gl = api();
  gl.Disable(kPolygonOffsetFill);
  gl.PolygonOffset(0.0f, 0.0f);
  gl.BindFramebuffer(kFramebuffer, 0);
  gl.Viewport(previousViewport_[0], previousViewport_[1],
              previousViewport_[2], previousViewport_[3]);
  gl.UseProgram(0);
}

const float* Gl33ShadowMap::lightView() const {
  return view_.data();
}

const float* Gl33ShadowMap::lightProjection() const {
  return projection_.data();
}

void Gl33ShadowMap::applyToReceiver(const Gl33ShaderProgram& program) const {
  program.setMatrix4("uShadowViewProjection", viewProjection_.data());
  program.setInt("uShadowMap", kWorld11ShadowTextureUnit);
  program.setInt("uShadowEnabled", depthTexture_ != 0 ? 1 : 0);
  program.setFloat("uShadowTexelSize",
                   1.0f / static_cast<float>(kShadowMapSize));
  program.setFloat("uShadowDepthBias", depthBias_);
  program.setFloat("uShadowStrength", kShadowStrength);
  Gl33Api& gl = api();
  gl.ActiveTexture(kTexture0 + static_cast<Enum>(kWorld11ShadowTextureUnit));
  gl.BindTexture(kTexture2D, depthTexture_);
  gl.ActiveTexture(kTexture0);
}

void Gl33ShadowMap::unbind() const {
  Gl33Api& gl = api();
  gl.ActiveTexture(kTexture0 + static_cast<Enum>(kWorld11ShadowTextureUnit));
  gl.BindTexture(kTexture2D, 0);
  gl.ActiveTexture(kTexture0);
}

void Gl33ShadowMap::ensureInitialized() {
  if (framebuffer_ != 0) {
    return;
  }
  Gl33Api& gl = api();
  gl.GenTextures(1, &depthTexture_);
  gl.BindTexture(kTexture2D, depthTexture_);
  // Linear filtering with reference comparison gives hardware 2x2 PCF.
  gl.TexParameteri(kTexture2D, kTextureMinFilter, static_cast<Int>(kLinear));
  gl.TexParameteri(kTexture2D, kTextureMagFilter, static_cast<Int>(kLinear));
  gl.TexParameteri(kTexture2D, kTextureWrapS, static_cast<Int>(kClampToEdge));
  gl.TexParameteri(kTexture2D, kTextureWrapT, static_cast<Int>(kClampToEdge));
  gl.TexParameteri(kTexture2D, kTextureCompareMode,
                   static_cast<Int>(kCompareRefToTexture));
  gl.TexParameteri(kTexture2D, kTextureCompareFunc,
                   static_cast<Int>(kLessEqual));
  gl.TexImage2D(kTexture2D, 0, kDepthComponent24, kShadowMapSize,
                kShadowMapSize, 0, kDepthComponent, kUnsignedInt, nullptr);
  gl.BindTexture(kTexture2D, 0);

  gl.GenFramebuffers(1, &framebuffer_);
  gl.BindFramebuffer(kFramebuffer, framebuffer_);
  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment, kTexture2D,
                          depthTexture_, 0);
  gl.DrawBuffer(kNone);
  gl.ReadBuffer(kNone);
  const Enum status = gl.CheckFramebufferStatus(kFramebuffer);
  gl.BindFramebuffer(kFramebuffer, 0);
  if (status != kFramebufferComplete) {
    throw std::runtime_error("World 11 shadow framebuffer is incomplete");
  }
}

}  // namespace hg::render::gl33
