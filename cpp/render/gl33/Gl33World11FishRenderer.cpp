#include "render/gl33/Gl33World11FishRenderer.hpp"
#include "render/gl33/World11Environment.hpp"

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Mesh.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33ShadowMap.hpp"
#include "render/gl33/Gl33Texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kBodySurface = 0.0f;
constexpr float kCutoutSurface = 1.0f;

struct Vec3 {
  float x;
  float y;
  float z;
};

using Matrix4 = std::array<float, 16>;

float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 multiply(const Vec3& value, float factor) {
  return {value.x * factor, value.y * factor, value.z * factor};
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

float length(const Vec3& value) {
  return std::sqrt(dot(value, value));
}

Vec3 normalize(const Vec3& value, const Vec3& fallback) {
  const float valueLength = length(value);
  if (valueLength < 1.0e-6f) {
    return fallback;
  }
  return multiply(value, 1.0f / valueLength);
}

Matrix4 perspective(float fovRadians, float aspect,
                    float nearPlane, float farPlane) {
  const float inverseTangent = 1.0f / std::tan(fovRadians * 0.5f);
  Matrix4 matrix{};
  matrix[0] = inverseTangent / aspect;
  matrix[5] = inverseTangent;
  matrix[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
  matrix[11] = -1.0f;
  matrix[14] = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
  return matrix;
}

Matrix4 viewMatrix(const Vec3& eye, const Vec3& front,
                   const Vec3& cameraUp) {
  const Vec3 forward = normalize(front, {0.0f, 0.0f, -1.0f});
  const Vec3 right = normalize(cross(forward, cameraUp), {1.0f, 0.0f, 0.0f});
  const Vec3 up = normalize(cross(right, forward), {0.0f, 1.0f, 0.0f});
  return {
      right.x, up.x, -forward.x, 0.0f,
      right.y, up.y, -forward.y, 0.0f,
      right.z, up.z, -forward.z, 0.0f,
      -dot(right, eye), -dot(up, eye), dot(forward, eye), 1.0f,
  };
}

struct FishBodySection {
  float textureU;
  float topV;
  float bottomV;
  bool active;
};

float localYFromV(float textureV, float halfHeight) {
  return halfHeight * (1.0f - 2.0f * textureV);
}

bool isDenseBodyPixel(const Gl33Texture& texture, int centerX, int centerY) {
  constexpr int kRadiusX = 3;
  constexpr int kRadiusY = 2;
  constexpr float kAlphaThreshold = 0.34f;
  constexpr int kRequiredSamples = 19;

  const int width = texture.width();
  const int height = texture.height();
  int acceptedSamples = 0;

  for (int dy = -kRadiusY; dy <= kRadiusY; ++dy) {
    const int y = std::clamp(centerY + dy, 0, height - 1);
    for (int dx = -kRadiusX; dx <= kRadiusX; ++dx) {
      const int x = std::clamp(centerX + dx, 0, width - 1);
      if (texture.alphaAt(x, y) >= kAlphaThreshold) {
        ++acceptedSamples;
      }
    }
  }

  return acceptedSamples >= kRequiredSamples;
}

std::vector<FishBodySection> extractBodySections(
    const Gl33Texture& texture, std::size_t sectionCount) {
  std::vector<FishBodySection> sections(sectionCount);
  const int width = texture.width();
  const int height = texture.height();

  for (std::size_t sectionIndex = 0; sectionIndex < sectionCount;
       ++sectionIndex) {
    const float u = sectionCount > 1U
        ? static_cast<float>(sectionIndex) /
              static_cast<float>(sectionCount - 1U)
        : 0.0f;
    const int centerX = std::clamp(
        static_cast<int>(std::lround(u * static_cast<float>(width - 1))),
        0, width - 1);

    int bestStart = -1;
    int bestEnd = -1;
    int bestLength = 0;
    int runStart = -1;

    for (int y = 0; y <= height; ++y) {
      const bool bodyPixel = y < height &&
          isDenseBodyPixel(texture, centerX, y);

      if (bodyPixel) {
        if (runStart < 0) {
          runStart = y;
        }
      } else if (runStart >= 0) {
        const int runEnd = y - 1;
        const int runLength = runEnd - runStart + 1;
        if (runLength > bestLength) {
          bestStart = runStart;
          bestEnd = runEnd;
          bestLength = runLength;
        }
        runStart = -1;
      }
    }

    if (bestLength < 4) {
      sections[sectionIndex] = {u, 0.5f, 0.5f, false};
      continue;
    }

    const int padding = std::max(1, height / 512);
    bestStart = std::max(0, bestStart - padding);
    bestEnd = std::min(height - 1, bestEnd + padding);
    sections[sectionIndex] = {
        u,
        static_cast<float>(bestStart) / static_cast<float>(height - 1),
        static_cast<float>(bestEnd) / static_cast<float>(height - 1),
        true};
  }

  for (int pass = 0; pass < 4; ++pass) {
    std::vector<FishBodySection> smoothed = sections;
    for (std::size_t index = 1; index + 1U < sections.size(); ++index) {
      if (!sections[index].active ||
          !sections[index - 1U].active ||
          !sections[index + 1U].active) {
        continue;
      }
      smoothed[index].topV =
          sections[index - 1U].topV * 0.22f +
          sections[index].topV * 0.56f +
          sections[index + 1U].topV * 0.22f;
      smoothed[index].bottomV =
          sections[index - 1U].bottomV * 0.22f +
          sections[index].bottomV * 0.56f +
          sections[index + 1U].bottomV * 0.22f;
    }
    sections.swap(smoothed);
  }

  return sections;
}

void appendSection(std::vector<Gl33Vertex>& vertices,
                   const FishBodySection& section,
                   float halfWidth,
                   float halfHeight) {
  const float x = halfWidth * (1.0f - 2.0f * section.textureU);
  const float topY = localYFromV(section.topV, halfHeight);
  const float bottomY = localYFromV(section.bottomV, halfHeight);
  const float centerY = 0.5f * (topY + bottomY);
  const float sectionHalfHeight = section.active
      ? std::max(0.5f * (topY - bottomY), halfHeight * 0.010f)
      : 0.0f;
  const float upperSideY = centerY + sectionHalfHeight * 0.36f;
  const float lowerSideY = centerY - sectionHalfHeight * 0.36f;

  const float tailTaper =
      1.0f - 0.92f * std::clamp(
          (section.textureU - 0.76f) / 0.24f, 0.0f, 1.0f);
  const float headTaper = 0.78f + 0.22f * std::clamp(
      section.textureU / 0.18f, 0.0f, 1.0f);
  const float thickness = section.active
      ? std::max(sectionHalfHeight * 0.52f * tailTaper * headTaper, 0.002f)
      : 0.0f;

  const Vec3 topSlopeNormal = normalize(
      {0.0f, thickness, topY - upperSideY}, {0.0f, 1.0f, 0.0f});
  const Vec3 bottomSlopeNormal = normalize(
      {0.0f, -thickness, lowerSideY - bottomY}, {0.0f, -1.0f, 0.0f});

  const float upperV = 0.5f - upperSideY / (2.0f * halfHeight);
  const float lowerV = 0.5f - lowerSideY / (2.0f * halfHeight);

  vertices.push_back({{x, topY, 0.0f}, {0.0f, 1.0f, 0.0f},
                      {section.textureU, section.topV}, kBodySurface});
  vertices.push_back({{x, upperSideY, thickness},
                      {topSlopeNormal.x, topSlopeNormal.y, topSlopeNormal.z},
                      {section.textureU, upperV}, kBodySurface});
  vertices.push_back({{x, lowerSideY, thickness}, {0.0f, 0.0f, 1.0f},
                      {section.textureU, lowerV}, kBodySurface});
  vertices.push_back({{x, bottomY, 0.0f}, {0.0f, -1.0f, 0.0f},
                      {section.textureU, section.bottomV}, kBodySurface});
  vertices.push_back({{x, lowerSideY, -thickness},
                      {bottomSlopeNormal.x, bottomSlopeNormal.y,
                       -bottomSlopeNormal.z},
                      {section.textureU, lowerV}, kBodySurface});
  vertices.push_back({{x, upperSideY, -thickness},
                      {topSlopeNormal.x, topSlopeNormal.y,
                       -topSlopeNormal.z},
                      {section.textureU, upperV}, kBodySurface});

  vertices.push_back({{x, halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f},
                      {section.textureU, 0.0f}, kCutoutSurface});
  vertices.push_back({{x, topY, 0.0f}, {0.0f, 0.0f, 1.0f},
                      {section.textureU, section.topV}, kCutoutSurface});
  vertices.push_back({{x, halfHeight, 0.0f}, {0.0f, 0.0f, -1.0f},
                      {section.textureU, 0.0f}, kCutoutSurface});
  vertices.push_back({{x, topY, 0.0f}, {0.0f, 0.0f, -1.0f},
                      {section.textureU, section.topV}, kCutoutSurface});
  vertices.push_back({{x, bottomY, 0.0f}, {0.0f, 0.0f, 1.0f},
                      {section.textureU, section.bottomV}, kCutoutSurface});
  vertices.push_back({{x, -halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f},
                      {section.textureU, 1.0f}, kCutoutSurface});
  vertices.push_back({{x, bottomY, 0.0f}, {0.0f, 0.0f, -1.0f},
                      {section.textureU, section.bottomV}, kCutoutSurface});
  vertices.push_back({{x, -halfHeight, 0.0f}, {0.0f, 0.0f, -1.0f},
                      {section.textureU, 1.0f}, kCutoutSurface});
}

void appendQuad(std::vector<std::uint32_t>& indices,
                std::uint32_t a, std::uint32_t b,
                std::uint32_t c, std::uint32_t d) {
  indices.insert(indices.end(), {a, b, c, c, b, d});
}

void appendBodyCap(std::vector<std::uint32_t>& indices,
                   std::uint32_t base,
                   bool reverse) {
  for (std::uint32_t triangle = 1U; triangle + 1U < 6U; ++triangle) {
    if (reverse) {
      indices.insert(indices.end(), {base, base + triangle + 1U,
                                     base + triangle});
    } else {
      indices.insert(indices.end(), {base, base + triangle,
                                     base + triangle + 1U});
    }
  }
}

void uploadFishBody(Gl33Mesh& mesh,
                    const Gl33Texture& texture,
                    float halfWidth,
                    float halfHeight) {
  constexpr std::size_t kSectionCount = 72U;
  constexpr std::uint32_t kVerticesPerSection = 14U;
  constexpr std::uint32_t kBodyVerticesPerSection = 6U;
  const std::vector<FishBodySection> sections =
      extractBodySections(texture, kSectionCount);

  std::vector<Gl33Vertex> vertices;
  std::vector<std::uint32_t> indices;
  vertices.reserve(sections.size() * kVerticesPerSection);
  indices.reserve((sections.size() - 1U) * 60U);

  for (const FishBodySection& section : sections) {
    appendSection(vertices, section, halfWidth, halfHeight);
  }

  for (std::uint32_t section = 0;
       section + 1U < static_cast<std::uint32_t>(sections.size());
       ++section) {
    const std::uint32_t current = section * kVerticesPerSection;
    const std::uint32_t next = (section + 1U) * kVerticesPerSection;

    if (sections[section].active && sections[section + 1U].active) {
      for (std::uint32_t face = 0; face < kBodyVerticesPerSection; ++face) {
        const std::uint32_t nextFace = (face + 1U) % kBodyVerticesPerSection;
        appendQuad(indices, current + face, next + face,
                   current + nextFace, next + nextFace);
      }
    } else if (!sections[section].active && sections[section + 1U].active) {
      appendBodyCap(indices, next, false);
    } else if (sections[section].active && !sections[section + 1U].active) {
      appendBodyCap(indices, current, true);
    }

    appendQuad(indices, current + 6U, next + 6U, current + 7U, next + 7U);
    appendQuad(indices, current + 9U, next + 9U, current + 8U, next + 8U);
    appendQuad(indices, current + 10U, next + 10U,
               current + 11U, next + 11U);
    appendQuad(indices, current + 13U, next + 13U,
               current + 12U, next + 12U);
  }

  mesh.upload(vertices, indices);
}

constexpr char kFishVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aSurfaceType;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uFishPosition;
uniform vec3 uForward;
uniform vec3 uUp;
uniform vec3 uSide;
uniform float uTailPhase;
uniform float uFishScale;
uniform float uTailAmplitude;
uniform float uFishHalfWidth;

out vec2 vUv;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vLocalNormal;
flat out int vSurfaceType;

vec3 rotateAroundY(vec3 value, float angle) {
  float sine = sin(angle);
  float cosine = cos(angle);
  return vec3(
      cosine * value.x + sine * value.z,
      value.y,
      -sine * value.x + cosine * value.z);
}

vec3 rotatePointAroundY(vec3 point, float hingeX, float angle) {
  point.x -= hingeX;
  point = rotateAroundY(point, angle);
  point.x += hingeX;
  return point;
}

void main() {
  vec3 local = aPosition;
  const float firstJointU = 0.68;
  const float secondJointU = 0.80;
  const float thirdJointU = 0.90;
  float firstHingeX = uFishHalfWidth * (1.0 - 2.0 * firstJointU);
  float secondHingeX = uFishHalfWidth * (1.0 - 2.0 * secondJointU);
  float thirdHingeX = uFishHalfWidth * (1.0 - 2.0 * thirdJointU);

  float firstAngle = radians(6.5) * sin(uTailPhase) * uTailAmplitude;
  float secondAngle = radians(8.5) *
      sin(uTailPhase - 0.38) * uTailAmplitude;
  float thirdAngle = radians(10.0) *
      sin(uTailPhase - 0.72) * uTailAmplitude;

  if (aUv.x >= thirdJointU) {
    local = rotatePointAroundY(local, thirdHingeX, thirdAngle);
  }
  if (aUv.x >= secondJointU) {
    local = rotatePointAroundY(local, secondHingeX, secondAngle);
  }
  if (aUv.x >= firstJointU) {
    local = rotatePointAroundY(local, firstHingeX, firstAngle);
  }

  float normalAngle = 0.0;
  if (aUv.x >= firstJointU) normalAngle += firstAngle;
  if (aUv.x >= secondJointU) normalAngle += secondAngle;
  if (aUv.x >= thirdJointU) normalAngle += thirdAngle;
  vec3 localNormal = normalize(rotateAroundY(aNormal, normalAngle));

  local *= uFishScale;
  vec3 worldPosition = uFishPosition +
      uForward * local.x + uUp * local.y + uSide * local.z;
  vUv = aUv;
  vLocalNormal = localNormal;
  vSurfaceType = int(aSurfaceType + 0.5);
  vWorldPosition = worldPosition;
  vWorldNormal = normalize(
      uForward * localNormal.x +
      uUp * localNormal.y +
      uSide * localNormal.z);
  gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)glsl";

constexpr char kFishFragmentShader[] = R"glsl(#version 330 core
in vec2 vUv;
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vLocalNormal;
flat in int vSurfaceType;

uniform sampler2D uFishTexture;
uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform vec3 uFishTint;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;
uniform float uAlphaCutoff;

out vec4 FragColor;

void main() {
  if (!gl_FrontFacing) {
    discard;
  }

  vec4 texel = texture(uFishTexture, vUv);
  bool cutoutSurface = vSurfaceType == 1;

  if (cutoutSurface && texel.a < uAlphaCutoff) {
    discard;
  }

  vec4 centerTexel = texture(uFishTexture, vec2(vUv.x, 0.5));
  float bodyTextureWeight = smoothstep(0.05, 0.35, texel.a);
  vec3 bodyAlbedo = mix(centerTexel.rgb, texel.rgb, bodyTextureWeight);
  vec3 albedo = (cutoutSurface ? texel.rgb : bodyAlbedo) * uFishTint;

  vec3 normal = normalize(vWorldNormal);
  vec3 lightDirection = normalize(vec3(0.32, 0.88, 0.36));
  float shadow = world11ShadowVisibility(
      vWorldPosition, normal, -lightDirection);
  float diffuse = 0.54 + 0.46 * max(dot(normal, lightDirection), 0.0) * shadow;
  float rim = pow(
      1.0 - abs(dot(normal, normalize(uCameraPosition - vWorldPosition))),
      2.0);
  float waterDepth = max(0.0, uWaterSurfaceY - vWorldPosition.y);
  vec3 absorption = exp(-vec3(0.010, 0.0045, 0.0022) * waterDepth);
  vec3 color = albedo * diffuse * absorption;
  color += vec3(0.018, 0.045, 0.052) + rim * vec3(0.022, 0.050, 0.055);
  float distanceToCamera = distance(vWorldPosition, uCameraPosition);
  float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
  float meanDepth = min(24.0, 0.5 * (cameraDepth + waterDepth));
  float fogFactor = 1.0 - exp(
      -(uAbsorptionDensity + uDepthAbsorption * meanDepth) * distanceToCamera);
  FragColor = vec4(mix(color, uFogColor, fogFactor), 1.0);
}
)glsl";

// Both faces cast; fins and tail keep their texture cutout in the shadow.
constexpr char kFishShadowFragmentShader[] = R"glsl(#version 330 core
in vec2 vUv;
flat in int vSurfaceType;

uniform sampler2D uFishTexture;
uniform float uAlphaCutoff;

void main() {
  if (vSurfaceType == 1 && texture(uFishTexture, vUv).a < uAlphaCutoff) {
    discard;
  }
}
)glsl";

}  // namespace

struct Gl33World11FishRenderer::Resources {
  Gl33Mesh fishMesh;
  Gl33ShaderProgram fishProgram;
  Gl33ShaderProgram fishShadowProgram;
  Gl33Texture fishTexture;
  float fishHalfWidth = 0.0f;
};

Gl33World11FishRenderer::Gl33World11FishRenderer(
    std::uint64_t trajectorySeed) {
  trajectories_.reserve(6);
  trajectories_.emplace_back(trajectorySeed);
  for (int i = 1; i < 6; ++i) {
    world::World11FishMovementVolume volume;
    volume.center = {-8.0f + i * 4.0f, 1.0f + (i % 3), -42.0f - (i % 3) * 8.0f};
    // Keep the full-size clearance as a conservative bound for smaller fish.
    trajectories_.emplace_back(trajectorySeed ^
        (0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(i)),
        volume.center, volume);
  }
}

Gl33World11FishRenderer::~Gl33World11FishRenderer() = default;

void Gl33World11FishRenderer::render(
    const Gl33Camera& camera, float simulationTime,
    const Gl33ShadowMap* shadowMap) {
  ensureInitialized();
  const Vec3 cameraPosition{
      camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 cameraFront{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 cameraUp{camera.up[0], camera.up[1], camera.up[2]};
  const float aspect = static_cast<float>(std::max(camera.framebufferWidth, 1)) /
      static_cast<float>(std::max(camera.framebufferHeight, 1));
  const Matrix4 view = viewMatrix(cameraPosition, cameraFront, cameraUp);
  const Matrix4 projection = perspective(
      60.0f * kPi / 180.0f, aspect, 0.1f, 520.0f);

  Gl33Api& gl = api();
  gl.Enable(kDepthTest);
  gl.DepthMask(kTrue);
  gl.Disable(kBlend);
  gl.Disable(kCullFace);

  resources_->fishProgram.use();
  resources_->fishProgram.setMatrix4("uView", view.data());
  resources_->fishProgram.setMatrix4("uProjection", projection.data());
  resources_->fishProgram.setFloat("uFishHalfWidth", resources_->fishHalfWidth);
  resources_->fishProgram.setVec3(
      "uCameraPosition", cameraPosition.x, cameraPosition.y, cameraPosition.z);
  setWorld11Environment(resources_->fishProgram);
  if (shadowMap != nullptr) {
    shadowMap->applyToReceiver(resources_->fishProgram);
  } else {
    resources_->fishProgram.setInt("uShadowEnabled", 0);
  }
  resources_->fishProgram.setFloat("uAlphaCutoff", 0.30f);
  resources_->fishProgram.setInt("uFishTexture", 0);
  resources_->fishTexture.bind(0);
  drawPopulation(resources_->fishProgram, simulationTime);
  gl.BindTexture(kTexture2D, 0);
  gl.UseProgram(0);
}

void Gl33World11FishRenderer::renderShadowDepth(
    float simulationTime, const Gl33ShadowMap& shadowMap) {
  ensureInitialized();
  const Gl33ShaderProgram& program = resources_->fishShadowProgram;
  program.use();
  program.setMatrix4("uView", shadowMap.lightView());
  program.setMatrix4("uProjection", shadowMap.lightProjection());
  program.setFloat("uFishHalfWidth", resources_->fishHalfWidth);
  program.setFloat("uAlphaCutoff", 0.30f);
  program.setInt("uFishTexture", 0);
  resources_->fishTexture.bind(0);
  drawPopulation(program, simulationTime);
  Gl33Api& gl = api();
  gl.BindTexture(kTexture2D, 0);
  gl.UseProgram(0);
}

void Gl33World11FishRenderer::drawPopulation(
    const Gl33ShaderProgram& program, float simulationTime) {
  for (std::size_t index = 0; index < trajectories_.size(); ++index) {
    const world::World11FishState fish = trajectories_[index].sample(
        std::max(0.0f, simulationTime));
    const Vec3 velocity{fish.velocity.x, fish.velocity.y, fish.velocity.z};
    const float speed = length(velocity);
    const Vec3 forward = normalize(velocity, {1.0f, 0.0f, 0.0f});
    const Vec3 approximateUp = normalize(subtract(
        {0.0f, 1.0f, 0.0f}, multiply(forward, forward.y)), {0.0f, 1.0f, 0.0f});
    const Vec3 side = normalize(cross(forward, approximateUp), {0.0f, 0.0f, 1.0f});
    const Vec3 up = normalize(cross(side, forward), {0.0f, 1.0f, 0.0f});
    program.setFloat("uFishScale", index == 0 ? 1.0f :
        0.52f + 0.065f * static_cast<float>(index));
    program.setVec3("uFishTint", index % 2 == 0 ? 1.08f : 0.78f,
        0.94f, index % 2 == 0 ? 0.76f : 1.08f);
    program.setVec3(
        "uFishPosition", fish.position.x, fish.position.y, fish.position.z);
    program.setVec3("uForward", forward.x, forward.y, forward.z);
    program.setVec3("uUp", up.x, up.y, up.z);
    program.setVec3("uSide", side.x, side.y, side.z);
    program.setFloat("uTailPhase", fish.tailPhase);
    program.setFloat(
        "uTailAmplitude", 0.84f + 0.16f * std::clamp(
            speed / world::World11FishTrajectory::kMaximumSpeed, 0.0f, 1.0f));
    resources_->fishMesh.draw();
  }
}

void Gl33World11FishRenderer::ensureInitialized() {
  if (resources_ != nullptr) {
    return;
  }
  resources_ = std::make_unique<Resources>();
  const std::string fishFragmentShader =
      withWorld11Shadows(kFishFragmentShader);
  resources_->fishProgram.build(kFishVertexShader, fishFragmentShader.c_str());
  initializeWorld11ShadowReceiver(resources_->fishProgram);
  resources_->fishShadowProgram.build(kFishVertexShader,
                                      kFishShadowFragmentShader);
  resources_->fishTexture.loadFget(
      "datasets/0x00000019.fget", Gl33TextureWrap::ClampToEdge);
  constexpr float kFishHalfHeight = 0.65f;
  const float textureAspect =
      static_cast<float>(resources_->fishTexture.width()) /
      static_cast<float>(resources_->fishTexture.height());
  resources_->fishHalfWidth = kFishHalfHeight * textureAspect;
  uploadFishBody(resources_->fishMesh,
                 resources_->fishTexture,
                 resources_->fishHalfWidth,
                 kFishHalfHeight);
}

}  // namespace hg::render::gl33
