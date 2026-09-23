#include "render/gl33/Gl33World11DecorRenderer.hpp"
#include "render/gl33/World11Environment.hpp"
#include "world/World11Landmarks.hpp"
#include "world/World11Seabed.hpp"

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Buffer.hpp"
#include "render/gl33/Gl33Mesh.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33ShadowMap.hpp"
#include "render/gl33/Gl33Texture.hpp"
#include "render/gl33/Gl33VertexArray.hpp"
#include "render/gl33/World11CoralGeometry.hpp"
#include "render/gl33/World11WaterWaves.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr int kVisibleChunkRadius = 6;
constexpr int kRetainedChunkRadius = 9;

struct Vec3 {
  float x;
  float y;
  float z;
};

using Matrix4 = std::array<float, 16>;

struct MeshData {
  std::vector<Gl33Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

struct GpuDecorInstance {
  float position[3];
  float seabedNormal[3];
  float size[3];
  float yawPhaseAmplitude[3];
  float color[4];
  float appearance[4];
};
static_assert(sizeof(GpuDecorInstance) == 20 * sizeof(float),
              "Instance comparisons require packed float attributes");

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 multiply(const Vec3& value, float factor) {
  return {value.x * factor, value.y * factor, value.z * factor};
}

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
  const Vec3 forward = normalize(front);
  const Vec3 right = normalize(cross(forward, cameraUp));
  const Vec3 up = normalize(cross(right, forward));
  return {
      right.x, up.x, -forward.x, 0.0f,
      right.y, up.y, -forward.y, 0.0f,
      right.z, up.z, -forward.z, 0.0f,
      -dot(right, eye), -dot(up, eye), dot(forward, eye), 1.0f,
  };
}

MeshData makeSeaweedRibbon() {
  constexpr int segmentCount = 8;
  MeshData mesh;
  mesh.vertices.reserve((segmentCount + 1) * 2);
  mesh.indices.reserve(segmentCount * 6);
  for (int segment = 0; segment <= segmentCount; ++segment) {
    const float height = static_cast<float>(segment) /
        static_cast<float>(segmentCount);
    const float halfWidth = 0.5f * (1.0f - 0.68f * height * height);
    mesh.vertices.push_back(Gl33Vertex{
        {-halfWidth, height, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, height}});
    mesh.vertices.push_back(Gl33Vertex{
        {halfWidth, height, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, height}});
  }
  for (int segment = 0; segment < segmentCount; ++segment) {
    const std::uint32_t first = static_cast<std::uint32_t>(segment * 2);
    mesh.indices.insert(mesh.indices.end(), {
        first, first + 2U, first + 1U,
        first + 1U, first + 2U, first + 3U,
    });
  }
  return mesh;
}

MeshData makeBubbleMesh() {
  constexpr std::array<Vec3, 6> positions = {{
      {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
      {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
  }};
  constexpr std::array<std::array<std::uint32_t, 3>, 8> faces = {{
      {{0, 5, 3}}, {{0, 3, 4}}, {{0, 4, 2}}, {{0, 2, 5}},
      {{1, 3, 5}}, {{1, 4, 3}}, {{1, 2, 4}}, {{1, 5, 2}},
  }};
  MeshData mesh;
  for (const auto& face : faces) {
    for (const std::uint32_t vertexIndex : face) {
      const Vec3& position = positions[vertexIndex];
      mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size()));
      mesh.vertices.push_back(Gl33Vertex{
          {position.x, position.y, position.z},
          {position.x, position.y, position.z},
          {position.x * 0.5f + 0.5f, position.y * 0.5f + 0.5f},
      });
    }
  }
  return mesh;
}

GpuDecorInstance makeGpuInstance(const world::World11DecorInstance& source) {
  return GpuDecorInstance{
      {source.x, source.y, source.z},
      {source.normalX, source.normalY, source.normalZ},
      {source.width, source.height, source.depth},
      {source.yaw, source.phase, source.amplitude},
      {source.red, source.green, source.blue, 1.0f},
      {0.0f, 1.0f, 1.0f, 2.0f},
  };
}

float hashUnit(std::uint64_t value) {
  return static_cast<float>(value >> 40U) * (1.0f / 16777216.0f);
}

GpuDecorInstance makeCoralGpuInstance(
    const world::World11DecorInstance& source, float lodDither) {
  GpuDecorInstance result = makeGpuInstance(source);
  std::uint64_t appearanceSeed = world::World11DecorGenerator::stableHash(
      source.stableId, 0x434F52414C434F4CULL);
  result.appearance[0] = -0.14f + 0.28f * hashUnit(appearanceSeed);
  appearanceSeed = world::World11DecorGenerator::stableHash(
      appearanceSeed, 0x4855455348494654ULL);
  result.appearance[1] = 0.88f + 0.24f * hashUnit(appearanceSeed);
  appearanceSeed = world::World11DecorGenerator::stableHash(
      appearanceSeed, 0x4252494748544E53ULL);
  result.appearance[2] = 0.86f + 0.26f * hashUnit(appearanceSeed);
  result.appearance[3] = lodDither;
  return result;
}

std::size_t coralMeshVariantIndex(
    const world::World11DecorInstance& source, std::uint64_t coralSeed) {
  return static_cast<std::size_t>(world::World11DecorGenerator::stableHash(
      coralSeed, source.stableId) % CoralMeshSet::kVariantsPerType);
}

float smoothstep(float edge0, float edge1, float value) {
  const float unit = std::max(
      0.0f, std::min((value - edge0) / (edge1 - edge0), 1.0f));
  return unit * unit * (3.0f - 2.0f * unit);
}

struct CoralLodSelection {
  std::array<int, 2> lod{{0, 0}};
  std::array<float, 2> dither{{2.0f, 2.0f}};
  std::size_t count = 0;
};

CoralLodSelection selectCoralLods(float distanceToCamera, int& lodState) {
  CoralLodSelection selection;
  if (distanceToCamera >= 110.0f) {
    return selection;
  }
  if (distanceToCamera >= 91.5f) {
    return selection;
  }
  if (distanceToCamera >= 88.5f) {
    selection.lod[0] = 2;
    selection.dither[0] = smoothstep(88.5f, 91.5f, distanceToCamera);
    selection.count = 1;
    return selection;
  }

  if (lodState < 0 || lodState > 2) {
    lodState = distanceToCamera < 18.0f ? 0 :
        (distanceToCamera < 45.0f ? 1 : 2);
  }
  if (lodState == 0 && distanceToCamera > 20.0f) {
    lodState = 1;
  } else if (lodState == 1) {
    if (distanceToCamera < 16.0f) {
      lodState = 0;
    } else if (distanceToCamera > 47.0f) {
      lodState = 2;
    }
  } else if (lodState == 2 && distanceToCamera < 43.0f) {
    lodState = 1;
  }

  auto transition = [&selection](int nearLod, int farLod, float amount) {
    selection.lod[0] = nearLod;
    selection.dither[0] = amount;
    selection.lod[1] = farLod;
    selection.dither[1] = -(amount + 1.0f);
    selection.count = 2;
  };
  if (lodState == 0) {
    if (distanceToCamera >= 17.0f) {
      transition(0, 1, smoothstep(17.0f, 20.0f, distanceToCamera));
    } else {
      selection.lod[0] = 0;
      selection.count = 1;
    }
  } else if (lodState == 1) {
    if (distanceToCamera <= 19.0f) {
      transition(0, 1, smoothstep(16.0f, 19.0f, distanceToCamera));
    } else if (distanceToCamera >= 44.0f) {
      transition(1, 2, smoothstep(44.0f, 47.0f, distanceToCamera));
    } else {
      selection.lod[0] = 1;
      selection.count = 1;
    }
  } else if (distanceToCamera <= 46.0f) {
    transition(1, 2, smoothstep(43.0f, 46.0f, distanceToCamera));
  } else {
    selection.lod[0] = 2;
    selection.count = 1;
  }
  return selection;
}

constexpr char kSeaweedVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 iPosition;
layout(location = 4) in vec3 iSeabedNormal;
layout(location = 5) in vec3 iSize;
layout(location = 6) in vec3 iYawPhaseAmplitude;
layout(location = 7) in vec4 iColor;

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;

void instanceBasis(out vec3 tangent, out vec3 up, out vec3 bitangent) {
  up = normalize(iSeabedNormal);
  vec3 helper = abs(up.y) < 0.96 ? vec3(0.0, 1.0, 0.0)
                                  : vec3(1.0, 0.0, 0.0);
  tangent = normalize(cross(helper, up));
  bitangent = normalize(cross(up, tangent));
}

vec3 yawRotate(vec3 value, float angle) {
  float sine = sin(angle);
  float cosine = cos(angle);
  return vec3(cosine * value.x - sine * value.z,
              value.y,
              sine * value.x + cosine * value.z);
}

void main() {
  float heightFactor = aUv.y;
  float flexibleHeight = pow(heightFactor, 1.72);
  float phase = uTime * 0.82 + iYawPhaseAmplitude.y +
      dot(iPosition.xz, vec2(0.037, -0.029));
  float sway = sin(phase) * iYawPhaseAmplitude.z * flexibleHeight;
  float crossSway = cos(phase * 0.73 + 1.4) *
      iYawPhaseAmplitude.z * 0.44 * flexibleHeight;
  float staticLean = iSize.z * iSize.y * pow(heightFactor, 1.18);

  vec3 localPosition = vec3(aPosition.x * iSize.x + sway + staticLean,
                            aPosition.y * iSize.y,
                            aPosition.z + crossSway);
  localPosition = yawRotate(localPosition, iYawPhaseAmplitude.x);
  vec3 localNormal = yawRotate(aNormal, iYawPhaseAmplitude.x);

  vec3 tangent;
  vec3 up;
  vec3 bitangent;
  instanceBasis(tangent, up, bitangent);
  vec3 worldOffset = tangent * localPosition.x + up * localPosition.y +
      bitangent * localPosition.z;
  vec3 worldNormal = normalize(tangent * localNormal.x + up * localNormal.y +
                               bitangent * localNormal.z);
  vWorldPosition = iPosition + worldOffset;
  vNormal = worldNormal;
  vColor = iColor;
  gl_Position = uProjection * uView * vec4(vWorldPosition, 1.0);
}
)glsl";

constexpr char kCoralVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aCoralData;
layout(location = 3) in vec3 iPosition;
layout(location = 4) in vec3 iSeabedNormal;
layout(location = 5) in vec3 iSize;
layout(location = 6) in vec3 iYawPhaseAmplitude;
layout(location = 7) in vec4 iColor;
layout(location = 8) in vec4 iAppearance;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;
out vec3 vCoralData;
flat out vec4 vAppearance;

void main() {
  vec3 up = normalize(iSeabedNormal);
  vec3 helper = abs(up.y) < 0.96 ? vec3(0.0, 1.0, 0.0)
                                  : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(helper, up));
  vec3 bitangent = normalize(cross(up, tangent));
  float sine = sin(iYawPhaseAmplitude.x);
  float cosine = cos(iYawPhaseAmplitude.x);
  vec3 scaled = aPosition * iSize;
  vec3 localPosition = vec3(cosine * scaled.x - sine * scaled.z,
                            scaled.y,
                            sine * scaled.x + cosine * scaled.z);
  vec3 inverseScaledNormal = aNormal / max(iSize, vec3(0.0001));
  vec3 localNormal = vec3(
      cosine * inverseScaledNormal.x - sine * inverseScaledNormal.z,
      inverseScaledNormal.y,
      sine * inverseScaledNormal.x + cosine * inverseScaledNormal.z);
  vWorldPosition = iPosition + tangent * localPosition.x +
      up * localPosition.y + bitangent * localPosition.z;
  vNormal = normalize(tangent * localNormal.x + up * localNormal.y +
                      bitangent * localNormal.z);
  vColor = iColor;
  vCoralData = aCoralData;
  vAppearance = iAppearance;
  gl_Position = uProjection * uView * vec4(vWorldPosition, 1.0);
}
)glsl";

constexpr char kBubbleVertexShaderPrefix[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 iPosition;
layout(location = 4) in vec3 iSeabedNormal;
layout(location = 5) in vec3 iSize;
layout(location = 6) in vec3 iYawPhaseAmplitude;
layout(location = 7) in vec4 iColor;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraPosition;
uniform float uTime;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;
out float vBubbleOpacity;
flat out vec3 vBubbleCenter;
flat out float vBubbleRadius;
flat out float vBubbleRoundness;
)glsl";

constexpr char kBubbleVertexShaderSuffix[] = R"glsl(
void main() {
  const float startOffset = 0.12;
  const float surfaceEpsilon = 0.035;
  const float collapseStartDistance = 0.11;
  const float collapseEndDistance = 0.05;
  const float roundBubbleDistance = 18.0;
  const float facetedBubbleDistance = 28.0;
  const float sphereEnvelopeScale = 1.74;

  float nominalRiseHeight = max(
      0.5, WORLD11_WATER_BASE_Y - iPosition.y - startOffset);
  float progress = fract(iYawPhaseAmplitude.y +
      uTime * iSize.y / nominalRiseHeight);
  vec2 direction = vec2(cos(iYawPhaseAmplitude.x),
                        sin(iYawPhaseAmplitude.x));
  vec2 perpendicular = vec2(-direction.y, direction.x);
  float phase = progress * 12.5663706 + iYawPhaseAmplitude.y * 6.2831853;
  vec2 drift = direction * sin(phase) * iYawPhaseAmplitude.z +
      perpendicular * cos(phase * 0.57) * iYawPhaseAmplitude.z * 0.38;
  vec2 centerXZ = iPosition.xz + drift;
  vec3 localSurface = world11WaterSurfaceAtWorldXZ(centerXZ);

  float bubbleRadius = iSize.x;
  float startCenterY = iPosition.y + startOffset;
  float surfaceCenterY = localSurface.y - surfaceEpsilon - bubbleRadius;
  float localRiseHeight = max(0.05, surfaceCenterY - startCenterY);
  float centerY = startCenterY + progress * localRiseHeight;
  float distanceToSurface = max(
      localSurface.y - surfaceEpsilon - (centerY + bubbleRadius), 0.0);
  float collapse = 1.0 - smoothstep(
      collapseEndDistance, collapseStartDistance, distanceToSurface);
  float radiusScale = mix(1.0, 1.28, collapse);

  vec3 center = vec3(centerXZ.x, centerY, centerXZ.y);
  float distanceToCamera = length(center - uCameraPosition);
  float roundness = 1.0 - smoothstep(
      roundBubbleDistance, facetedBubbleDistance, distanceToCamera);
  float envelopeScale = mix(1.0, sphereEnvelopeScale, roundness);
  float renderedRadius = bubbleRadius * radiusScale;

  vWorldPosition = center + aPosition * renderedRadius * envelopeScale;
  vNormal = normalize(aNormal);
  vColor = iColor;
  vBubbleOpacity = 1.0 - collapse;
  vBubbleCenter = center;
  vBubbleRadius = renderedRadius;
  vBubbleRoundness = roundness;
  gl_Position = uProjection * uView * vec4(vWorldPosition, 1.0);
}
)glsl";

std::string bubbleVertexShaderSource() {
  std::string source{kBubbleVertexShaderPrefix};
  source += kWorld11WaterWavesGlsl;
  source += kBubbleVertexShaderSuffix;
  return source;
}

constexpr char kDecorFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;

uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uFogColor;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;

out vec4 fragmentColor;

void main() {
  vec3 normal = gl_FrontFacing ? normalize(vNormal) : -normalize(vNormal);
  float diffuse = max(dot(normal, normalize(-uLightDirection)), 0.0);
  float shadow = world11ShadowVisibility(
      vWorldPosition, normal, uLightDirection);
  vec3 litColor = vColor.rgb * (0.25 + diffuse * 0.75 * shadow);
  float distanceToCamera = length(vWorldPosition - uCameraPosition);
  float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
  float fragmentDepth = max(0.0, uWaterSurfaceY - vWorldPosition.y);
  float meanDepth = min(24.0, 0.5 * (cameraDepth + fragmentDepth));
  float density = uAbsorptionDensity + uDepthAbsorption * meanDepth;
  float fogAmount = 1.0 - exp(-density * distanceToCamera);
  fragmentColor = vec4(mix(litColor, uFogColor, fogAmount), vColor.a);
}
)glsl";

constexpr char kCoralFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;
in vec3 vCoralData;
flat in vec4 vAppearance;

uniform sampler2D uCoralAlbedo;
uniform float uCoralTextureScale;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uFogColor;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;

out vec4 fragmentColor;

float bayer4x4() {
  const float pattern[16] = float[16](
       0.0,  8.0,  2.0, 10.0,
      12.0,  4.0, 14.0,  6.0,
       3.0, 11.0,  1.0,  9.0,
      15.0,  7.0, 13.0,  5.0);
  ivec2 pixel = ivec2(gl_FragCoord.xy) & ivec2(3);
  return (pattern[pixel.y * 4 + pixel.x] + 0.5) / 16.0;
}

void applyLodDither(float code) {
  float threshold = bayer4x4();
  if (code >= 0.0 && code <= 1.0) {
    if (threshold < code) {
      discard;
    }
  } else if (code < 0.0) {
    float incoming = clamp(-code - 1.0, 0.0, 1.0);
    if (threshold >= incoming) {
      discard;
    }
  }
}

vec3 rotateHue(vec3 color, float angle) {
  const vec3 axis = vec3(0.57735026919);
  float cosine = cos(angle);
  float sine = sin(angle);
  return color * cosine + cross(axis, color) * sine +
      axis * dot(axis, color) * (1.0 - cosine);
}

vec3 sampleCoralAlbedo(vec3 worldPosition, vec3 surfaceNormal) {
  vec3 weights = pow(abs(surfaceNormal), vec3(4.0));
  weights /= max(weights.x + weights.y + weights.z, 0.0001);
  vec3 xProjection = texture(
      uCoralAlbedo, worldPosition.yz * uCoralTextureScale).rgb;
  vec3 yProjection = texture(
      uCoralAlbedo, worldPosition.xz * uCoralTextureScale).rgb;
  vec3 zProjection = texture(
      uCoralAlbedo, worldPosition.xy * uCoralTextureScale).rgb;
  return xProjection * weights.x + yProjection * weights.y +
      zProjection * weights.z;
}

void main() {
  applyLodDither(vAppearance.w);
  vec3 normal = gl_FrontFacing ? normalize(vNormal) : -normalize(vNormal);
  vec3 lightDirection = normalize(-uLightDirection);
  const float wrap = 0.55;
  float diffuse = clamp(
      (dot(normal, lightDirection) + wrap) / (1.0 + wrap), 0.0, 1.0);

  float height = clamp(vCoralData.x, 0.0, 1.0);
  float branchLevel = clamp(vCoralData.y, 0.0, 1.0);
  float branchTint = vCoralData.z;
  float baseShade = mix(0.66, 1.0, smoothstep(0.0, 0.48, height));
  float youngBranchLight = branchLevel * 0.10;
  float tipHighlight = smoothstep(0.80, 1.0, height) *
      mix(0.05, 0.13, branchLevel);
  vec3 coralColor = vColor.rgb *
      (baseShade + youngBranchLight + tipHighlight + branchTint);
  vec3 tissueAlbedo = sampleCoralAlbedo(vWorldPosition, normal);
  float tissueLuminance = dot(
      tissueAlbedo, vec3(0.2126, 0.7152, 0.0722));
  vec3 tissueTint = clamp(
      tissueAlbedo / max(tissueLuminance, 0.08), vec3(0.72), vec3(1.28));
  coralColor *= mix(vec3(1.0), tissueTint, 0.18);
  coralColor *= mix(
      0.76, 1.20, smoothstep(0.28, 0.82, tissueLuminance));
  coralColor = max(rotateHue(coralColor, vAppearance.x), vec3(0.0));
  float luminance = dot(coralColor, vec3(0.2126, 0.7152, 0.0722));
  coralColor = mix(vec3(luminance), coralColor,
                   vAppearance.y * (1.0 + tipHighlight * 0.35));
  coralColor *= vAppearance.z;
  float shadow = world11ShadowVisibility(
      vWorldPosition, normal, uLightDirection);
  vec3 litColor = coralColor * (0.46 + diffuse * 0.54 * shadow);

  float distanceToCamera = length(vWorldPosition - uCameraPosition);
  float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
  float fragmentDepth = max(0.0, uWaterSurfaceY - vWorldPosition.y);
  float meanDepth = min(24.0, 0.5 * (cameraDepth + fragmentDepth));
  float density = uAbsorptionDensity + uDepthAbsorption * meanDepth;
  float fogAmount = 1.0 - exp(-density * distanceToCamera);
  fragmentColor = vec4(mix(litColor, uFogColor, fogAmount), 1.0);
}
)glsl";

constexpr char kBubbleFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;
in float vBubbleOpacity;
flat in vec3 vBubbleCenter;
flat in float vBubbleRadius;
flat in float vBubbleRoundness;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;
uniform sampler2D uWaterSurfaceDepth;
uniform int uBubblePass;
uniform float uWaterDepthBias;

out vec4 fragmentColor;

void main() {
  vec3 rayDirection = normalize(vWorldPosition - uCameraPosition);
  vec3 cameraToCenter = uCameraPosition - vBubbleCenter;
  float rayProjection = dot(cameraToCenter, rayDirection);
  float sphereEquation = dot(cameraToCenter, cameraToCenter) -
      vBubbleRadius * vBubbleRadius;
  float discriminant = rayProjection * rayProjection - sphereEquation;
  bool sphereHit = discriminant >= 0.0;
  float shapeCoverage = sphereHit ? 1.0 : 0.0;
  float shapeAlpha = mix(1.0, shapeCoverage, vBubbleRoundness);
  if (shapeAlpha <= 0.001) {
    discard;
  }

  vec3 shadingPosition = vWorldPosition;
  vec3 facetedNormal = gl_FrontFacing
      ? normalize(vNormal) : -normalize(vNormal);
  vec3 normal = facetedNormal;
  float effectiveDepth = gl_FragCoord.z;

  if (sphereHit && vBubbleRoundness > 0.0) {
    float root = sqrt(max(discriminant, 0.0));
    float nearDistance = -rayProjection - root;
    float farDistance = -rayProjection + root;
    float sphereDistance = nearDistance > 0.0 ? nearDistance : farDistance;
    if (sphereDistance > 0.0) {
      vec3 spherePosition = uCameraPosition +
          rayDirection * sphereDistance;
      vec3 sphereNormal = normalize(spherePosition - vBubbleCenter);
      vec4 sphereClip = uProjection * uView * vec4(spherePosition, 1.0);
      float sphereDepth = sphereClip.z / sphereClip.w * 0.5 + 0.5;
      shadingPosition = mix(
          vWorldPosition, spherePosition, vBubbleRoundness);
      normal = normalize(mix(
          facetedNormal, sphereNormal, vBubbleRoundness));
      effectiveDepth = mix(
          gl_FragCoord.z, sphereDepth, vBubbleRoundness);
    }
  }

  ivec2 depthSize = textureSize(uWaterSurfaceDepth, 0);
  ivec2 depthPixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), depthSize - 1);
  float waterDepth = texelFetch(uWaterSurfaceDepth, depthPixel, 0).r;
  bool hasWaterSurface = waterDepth < 1.0 - uWaterDepthBias;
  if (uBubblePass == 0) {
    if (!hasWaterSurface ||
        effectiveDepth <= waterDepth + uWaterDepthBias) {
      discard;
    }
  } else if (hasWaterSurface &&
             effectiveDepth >= waterDepth - uWaterDepthBias) {
    discard;
  }

  gl_FragDepth = effectiveDepth;
  vec3 toCamera = normalize(uCameraPosition - shadingPosition);
  float rim = pow(1.0 - abs(dot(normal, toCamera)), 1.7);
  vec3 bubbleColor = mix(vColor.rgb, vec3(0.88, 0.98, 1.0), rim);
  float alpha = (0.28 + rim * 0.48) *
      vBubbleOpacity * shapeAlpha;
  float distanceToCamera = length(shadingPosition - uCameraPosition);
  float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
  float fragmentDepth = max(0.0, uWaterSurfaceY - shadingPosition.y);
  float meanDepth = min(24.0, 0.5 * (cameraDepth + fragmentDepth));
  float density = uAbsorptionDensity + uDepthAbsorption * meanDepth;
  float fogAmount = 1.0 - exp(-density * distanceToCamera);
  fragmentColor = vec4(mix(bubbleColor, uFogColor, fogAmount),
                       alpha * (1.0 - fogAmount * 0.72));
}
)glsl";

constexpr char kShadowDepthFragmentShader[] = R"glsl(#version 330 core
void main() {
}
)glsl";

class InstancedMesh final {
public:
  InstancedMesh()
      : vertexBuffer_(kArrayBuffer), indexBuffer_(kElementArrayBuffer),
        instanceBuffer_(kArrayBuffer) {
  }

  void uploadGeometry(const MeshData& mesh) {
    vertexArray_.bind();
    vertexBuffer_.upload(mesh.vertices.data(), static_cast<SizePtr>(
        mesh.vertices.size() * sizeof(Gl33Vertex)));
    indexBuffer_.upload(mesh.indices.data(), static_cast<SizePtr>(
        mesh.indices.size() * sizeof(std::uint32_t)));

    Gl33Api& gl = api();
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, kFloat, kFalse,
        static_cast<SizeI>(sizeof(Gl33Vertex)),
        reinterpret_cast<const void*>(offsetof(Gl33Vertex, position)));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, kFloat, kFalse,
        static_cast<SizeI>(sizeof(Gl33Vertex)),
        reinterpret_cast<const void*>(offsetof(Gl33Vertex, normal)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 2, kFloat, kFalse,
        static_cast<SizeI>(sizeof(Gl33Vertex)),
        reinterpret_cast<const void*>(offsetof(Gl33Vertex, uv)));

    instanceBuffer_.bind();
    configureInstanceAttribute(3, 3, offsetof(GpuDecorInstance, position));
    configureInstanceAttribute(4, 3, offsetof(GpuDecorInstance, seabedNormal));
    configureInstanceAttribute(5, 3, offsetof(GpuDecorInstance, size));
    configureInstanceAttribute(6, 3, offsetof(GpuDecorInstance, yawPhaseAmplitude));
    configureInstanceAttribute(7, 4, offsetof(GpuDecorInstance, color));
    configureInstanceAttribute(8, 4, offsetof(GpuDecorInstance, appearance));
    gl.BindVertexArray(0);
    gl.BindBuffer(kArrayBuffer, 0);
    indexCount_ = static_cast<SizeI>(mesh.indices.size());
  }

  void uploadCoralGeometry(const CoralMeshData& mesh) {
    vertexArray_.bind();
    vertexBuffer_.upload(mesh.vertices.data(), static_cast<SizePtr>(
        mesh.vertices.size() * sizeof(CoralVertex)));
    indexBuffer_.upload(mesh.indices.data(), static_cast<SizePtr>(
        mesh.indices.size() * sizeof(std::uint32_t)));

    Gl33Api& gl = api();
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, kFloat, kFalse,
        static_cast<SizeI>(sizeof(CoralVertex)),
        reinterpret_cast<const void*>(offsetof(CoralVertex, position)));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, kFloat, kFalse,
        static_cast<SizeI>(sizeof(CoralVertex)),
        reinterpret_cast<const void*>(offsetof(CoralVertex, normal)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 3, kFloat, kFalse,
        static_cast<SizeI>(sizeof(CoralVertex)),
        reinterpret_cast<const void*>(offsetof(CoralVertex, coralData)));

    instanceBuffer_.bind();
    configureInstanceAttribute(3, 3, offsetof(GpuDecorInstance, position));
    configureInstanceAttribute(4, 3, offsetof(GpuDecorInstance, seabedNormal));
    configureInstanceAttribute(5, 3, offsetof(GpuDecorInstance, size));
    configureInstanceAttribute(6, 3,
                               offsetof(GpuDecorInstance, yawPhaseAmplitude));
    configureInstanceAttribute(7, 4, offsetof(GpuDecorInstance, color));
    configureInstanceAttribute(8, 4, offsetof(GpuDecorInstance, appearance));
    gl.BindVertexArray(0);
    gl.BindBuffer(kArrayBuffer, 0);
    indexCount_ = static_cast<SizeI>(mesh.indices.size());
  }

  void uploadInstances(const std::vector<GpuDecorInstance>& instances) {
    if (instances.size() == uploadedInstances_.size() &&
        (instances.empty() || std::memcmp(instances.data(), uploadedInstances_.data(),
            instances.size() * sizeof(GpuDecorInstance)) == 0)) {
      return;
    }
    const void* data = instances.empty() ? nullptr : instances.data();
    instanceBuffer_.upload(data, static_cast<SizePtr>(
        instances.size() * sizeof(GpuDecorInstance)), kDynamicDraw);
    instanceCount_ = static_cast<SizeI>(instances.size());
    uploadedInstances_ = instances;
  }

  void draw() const {
    if (indexCount_ == 0 || instanceCount_ == 0) {
      return;
    }
    vertexArray_.bind();
    api().DrawElementsInstanced(kTriangles, indexCount_, kUnsignedInt,
                                nullptr, instanceCount_);
    api().BindVertexArray(0);
  }

private:
  static void configureInstanceAttribute(UInt location, Int componentCount,
                                         std::size_t offset) {
    Gl33Api& gl = api();
    gl.EnableVertexAttribArray(location);
    gl.VertexAttribPointer(location, componentCount, kFloat, kFalse,
        static_cast<SizeI>(sizeof(GpuDecorInstance)),
        reinterpret_cast<const void*>(offset));
    gl.VertexAttribDivisor(location, 1U);
  }

  Gl33VertexArray vertexArray_;
  Gl33Buffer vertexBuffer_;
  Gl33Buffer indexBuffer_;
  Gl33Buffer instanceBuffer_;
  SizeI indexCount_ = 0;
  SizeI instanceCount_ = 0;
  std::vector<GpuDecorInstance> uploadedInstances_;
};

void setFrameUniforms(Gl33ShaderProgram& program, const Matrix4& view,
                      const Matrix4& projection, const Vec3& camera) {
  program.use();
  program.setMatrix4("uView", view.data());
  program.setMatrix4("uProjection", projection.data());
  program.setVec3("uCameraPosition", camera.x, camera.y, camera.z);
  program.setVec3("uLightDirection", -0.32f, -0.88f, -0.36f);
  setWorld11Environment(program);
}

}  // namespace

struct Gl33World11DecorRenderer::Resources {
  using CoralLodMeshes =
      std::array<InstancedMesh, CoralMeshSet::kLodCount>;
  using CoralVariantMeshes =
      std::array<CoralLodMeshes, CoralMeshSet::kVariantsPerType>;

  InstancedMesh seaweedMesh;
  std::array<CoralVariantMeshes, CoralMeshSet::kTypeCount> coralMeshes;
  InstancedMesh bubbleMesh;
  Gl33ShaderProgram seaweedProgram;
  Gl33ShaderProgram coralProgram;
  Gl33ShaderProgram bubbleProgram;
  Gl33ShaderProgram seaweedShadowProgram;
  Gl33ShaderProgram coralShadowProgram;
  Gl33Texture coralAlbedoTexture;
};

struct Gl33World11DecorRenderer::Cache {
  using LodGroups = std::array<std::vector<GpuDecorInstance>, CoralMeshSet::kLodCount>;
  using VariantGroups = std::array<LodGroups, CoralMeshSet::kVariantsPerType>;
  std::array<VariantGroups, CoralMeshSet::kTypeCount> coralGroups;
  std::array<float, 3> coralCamera{};
  bool coralCameraValid = false;
  std::unordered_map<std::uint64_t, world::World11DecorChunk> chunks;
  std::vector<std::uint64_t> activeKeys;
  std::unordered_map<std::uint64_t, int> coralLodState;
  int centerChunkX = std::numeric_limits<int>::max();
  int centerChunkZ = std::numeric_limits<int>::max();
};

Gl33World11DecorRenderer::Gl33World11DecorRenderer(
    world::World11DecorSeeds seeds)
    : generator_(seeds), cache_(std::make_unique<Cache>()) {
}

Gl33World11DecorRenderer::~Gl33World11DecorRenderer() = default;

void Gl33World11DecorRenderer::setSeeds(world::World11DecorSeeds seeds) {
  const std::uint64_t previousCoralSeed = generator_.seeds().coralSeed;
  generator_.setSeeds(seeds);
  if (generator_.seeds().coralSeed != previousCoralSeed) {
    resources_.reset();
  }
  cache_->chunks.clear();
  cache_->activeKeys.clear();
  cache_->coralLodState.clear();
  cache_->coralCameraValid = false;
  cache_->centerChunkX = std::numeric_limits<int>::max();
  cache_->centerChunkZ = std::numeric_limits<int>::max();
}

void Gl33World11DecorRenderer::render(const Gl33Camera& camera, float time,
                                      const Gl33ShadowMap* shadowMap) {
  ensureInitialized();
  updateVisibleChunks(camera);
  updateCoralInstances(camera);

  const Vec3 cameraPosition{
      camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const float aspect = static_cast<float>(std::max(camera.framebufferWidth, 1)) /
      static_cast<float>(std::max(camera.framebufferHeight, 1));
  const Matrix4 view = viewMatrix(cameraPosition, front, up);
  const Matrix4 projection = perspective(
      60.0f * kPi / 180.0f, aspect, 0.1f, 520.0f);

  Gl33Api& gl = api();
  gl.Enable(kDepthTest);
  gl.DepthMask(kTrue);
  gl.Disable(kBlend);
  gl.Disable(kCullFace);

  const auto applyShadows = [shadowMap](const Gl33ShaderProgram& program) {
    if (shadowMap != nullptr) {
      shadowMap->applyToReceiver(program);
    } else {
      program.setInt("uShadowEnabled", 0);
    }
  };

  setFrameUniforms(resources_->coralProgram, view, projection, cameraPosition);
  applyShadows(resources_->coralProgram);
  resources_->coralProgram.setInt("uCoralAlbedo", 1);
  resources_->coralProgram.setFloat("uCoralTextureScale", 1.25f);
  resources_->coralAlbedoTexture.bind(1);
  for (auto& typeMeshes : resources_->coralMeshes) {
    for (auto& variantMeshes : typeMeshes) {
      for (InstancedMesh& mesh : variantMeshes) {
        mesh.draw();
      }
    }
  }
  gl.BindTexture(kTexture2D, 0);
  gl.ActiveTexture(kTexture0);

  setFrameUniforms(resources_->seaweedProgram, view, projection, cameraPosition);
  applyShadows(resources_->seaweedProgram);
  resources_->seaweedProgram.setFloat("uTime", time);
  resources_->seaweedMesh.draw();
  gl.UseProgram(0);
}

void Gl33World11DecorRenderer::renderShadowDepth(
    const Gl33Camera& camera, float time, const Gl33ShadowMap& shadowMap) {
  ensureInitialized();
  updateVisibleChunks(camera);
  updateCoralInstances(camera);

  // LOD cross-fades draw both levels here without dithering; the overlap is
  // invisible in the filtered shadow.
  resources_->coralShadowProgram.use();
  resources_->coralShadowProgram.setMatrix4("uView", shadowMap.lightView());
  resources_->coralShadowProgram.setMatrix4(
      "uProjection", shadowMap.lightProjection());
  for (auto& typeMeshes : resources_->coralMeshes) {
    for (auto& variantMeshes : typeMeshes) {
      for (InstancedMesh& mesh : variantMeshes) {
        mesh.draw();
      }
    }
  }

  resources_->seaweedShadowProgram.use();
  resources_->seaweedShadowProgram.setMatrix4("uView", shadowMap.lightView());
  resources_->seaweedShadowProgram.setMatrix4(
      "uProjection", shadowMap.lightProjection());
  resources_->seaweedShadowProgram.setFloat("uTime", time);
  resources_->seaweedMesh.draw();
  api().UseProgram(0);
}

void Gl33World11DecorRenderer::renderBubbles(
    const Gl33Camera& camera, float time, Gl33BubblePass pass,
    std::uint32_t waterSurfaceDepthTexture) {
  ensureInitialized();
  updateVisibleChunks(camera);

  const Vec3 cameraPosition{
      camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const float aspect = static_cast<float>(std::max(camera.framebufferWidth, 1)) /
      static_cast<float>(std::max(camera.framebufferHeight, 1));
  const Matrix4 view = viewMatrix(cameraPosition, front, up);
  const Matrix4 projection = perspective(
      60.0f * kPi / 180.0f, aspect, 0.1f, 520.0f);

  Gl33Api& gl = api();
  gl.Enable(kDepthTest);
  gl.Enable(kBlend);
  gl.BlendFunc(kSourceAlpha, kOneMinusSourceAlpha);
  gl.DepthMask(kFalse);
  gl.Disable(kCullFace);
  setFrameUniforms(
      resources_->bubbleProgram, view, projection, cameraPosition);
  resources_->bubbleProgram.setFloat("uTime", time);
  resources_->bubbleProgram.setInt(
      "uBubblePass", static_cast<int>(pass));
  resources_->bubbleProgram.setInt("uWaterSurfaceDepth", 2);
  resources_->bubbleProgram.setFloat("uWaterDepthBias", 0.00005f);
  gl.ActiveTexture(kTexture0 + 2);
  gl.BindTexture(kTexture2D, waterSurfaceDepthTexture);
  resources_->bubbleMesh.draw();
  gl.BindTexture(kTexture2D, 0);
  gl.ActiveTexture(kTexture0);
  gl.DepthMask(kTrue);
  gl.UseProgram(0);
}

void Gl33World11DecorRenderer::ensureInitialized() {
  if (resources_ != nullptr) {
    return;
  }
  resources_ = std::make_unique<Resources>();
  const std::string decorFragmentShader =
      withWorld11Shadows(kDecorFragmentShader);
  const std::string coralFragmentShader =
      withWorld11Shadows(kCoralFragmentShader);
  resources_->seaweedProgram.build(kSeaweedVertexShader,
                                   decorFragmentShader.c_str());
  resources_->coralProgram.build(kCoralVertexShader,
                                 coralFragmentShader.c_str());
  initializeWorld11ShadowReceiver(resources_->seaweedProgram);
  initializeWorld11ShadowReceiver(resources_->coralProgram);
  resources_->seaweedShadowProgram.build(kSeaweedVertexShader,
                                         kShadowDepthFragmentShader);
  resources_->coralShadowProgram.build(kCoralVertexShader,
                                       kShadowDepthFragmentShader);
  resources_->coralAlbedoTexture.loadFget(
      "datasets/0x00000018.fget", Gl33TextureWrap::Repeat);
  const std::string bubbleShader = bubbleVertexShaderSource();
  resources_->bubbleProgram.build(bubbleShader.c_str(), kBubbleFragmentShader);
  resources_->seaweedMesh.uploadGeometry(makeSeaweedRibbon());
  resources_->bubbleMesh.uploadGeometry(makeBubbleMesh());
  const CoralMeshSet coralMeshSet(generator_.seeds().coralSeed);
  for (std::size_t type = 0; type < CoralMeshSet::kTypeCount; ++type) {
    for (std::size_t variant = 0;
         variant < CoralMeshSet::kVariantsPerType; ++variant) {
      const CoralMeshVariant& meshVariant = coralMeshSet.variant(type, variant);
      for (std::size_t lod = 0; lod < CoralMeshSet::kLodCount; ++lod) {
        resources_->coralMeshes[type][variant][lod].uploadCoralGeometry(
            meshVariant.lods[lod]);
      }
    }
  }
}

void Gl33World11DecorRenderer::updateVisibleChunks(const Gl33Camera& camera) {
  const int cameraChunkX = world::World11DecorGenerator::chunkCoordinate(
      camera.position[0]);
  const int cameraChunkZ = world::World11DecorGenerator::chunkCoordinate(
      camera.position[2]);
  if (cache_->centerChunkX == cameraChunkX &&
      cache_->centerChunkZ == cameraChunkZ) {
    return;
  }
  cache_->centerChunkX = cameraChunkX;
  cache_->centerChunkZ = cameraChunkZ;
  cache_->coralCameraValid = false;

  cache_->activeKeys.clear();
  for (int offsetZ = -kVisibleChunkRadius;
       offsetZ <= kVisibleChunkRadius; ++offsetZ) {
    for (int offsetX = -kVisibleChunkRadius;
         offsetX <= kVisibleChunkRadius; ++offsetX) {
      if (offsetX * offsetX + offsetZ * offsetZ >
          kVisibleChunkRadius * kVisibleChunkRadius) {
        continue;
      }
      const int chunkX = cameraChunkX + offsetX;
      const int chunkZ = cameraChunkZ + offsetZ;
      const std::uint64_t key = world::World11DecorGenerator::chunkKey(
          chunkX, chunkZ);
      cache_->activeKeys.push_back(key);
      if (cache_->chunks.find(key) == cache_->chunks.end()) {
        cache_->chunks.emplace(key, generator_.generateChunk(chunkX, chunkZ));
      }
    }
  }

  for (auto chunk = cache_->chunks.begin(); chunk != cache_->chunks.end();) {
    const int offsetX = chunk->second.chunkX - cameraChunkX;
    const int offsetZ = chunk->second.chunkZ - cameraChunkZ;
    if (std::abs(offsetX) > kRetainedChunkRadius ||
        std::abs(offsetZ) > kRetainedChunkRadius) {
      for (const world::World11DecorInstance& coral : chunk->second.coral) {
        cache_->coralLodState.erase(coral.stableId);
      }
      chunk = cache_->chunks.erase(chunk);
    } else {
      ++chunk;
    }
  }

  std::vector<GpuDecorInstance> seaweedInstances;
  std::vector<GpuDecorInstance> bubbleInstances;
  for (const std::uint64_t key : cache_->activeKeys) {
    const world::World11DecorChunk& chunk = cache_->chunks.at(key);
    for (const world::World11DecorInstance& instance : chunk.seaweed) {
      seaweedInstances.push_back(makeGpuInstance(instance));
    }
    for (const world::World11DecorInstance& instance : chunk.bubbles) {
      bubbleInstances.push_back(makeGpuInstance(instance));
    }
  }

  resources_->seaweedMesh.uploadInstances(seaweedInstances);
  const auto seep = world::kWorld11Seep;
  if (std::abs(world::World11DecorGenerator::chunkCoordinate(seep.x) - cameraChunkX) <= kVisibleChunkRadius &&
      std::abs(world::World11DecorGenerator::chunkCoordinate(seep.z) - cameraChunkZ) <= kVisibleChunkRadius) {
    for (int i = 0; i < 60; ++i) {
      const float angle = (i % 5) * 6.2831853f / 5.0f;
      world::World11DecorInstance bubble;
      bubble.type = world::World11DecorType::bubble;
      bubble.x = seep.x + 2.0f * std::cos(angle);
      bubble.z = seep.z + 2.0f * std::sin(angle);
      bubble.y = world::world11SeabedHeight(bubble.x, bubble.z) +
          (1.6f + 0.45f * ((i % 5) % 3)) * 1.4f;
      bubble.width = 0.055f + 0.014f * (i % 4);
      bubble.height = 0.8f + 0.07f * (i % 5);
      bubble.depth = 1.0f;
      bubble.phase = static_cast<float>(i / 5) / 12.0f;
      bubble.amplitude = 0.14f;
      bubble.yaw = angle;
      bubble.red = 0.62f; bubble.green = 0.94f; bubble.blue = 1.0f;
      bubbleInstances.push_back(makeGpuInstance(bubble));
    }
  }
  resources_->bubbleMesh.uploadInstances(bubbleInstances);
}

void Gl33World11DecorRenderer::updateCoralInstances(
    const Gl33Camera& camera) {
  const std::array<float, 3> position{
      camera.position[0], camera.position[1], camera.position[2]};
  if (cache_->coralCameraValid && position == cache_->coralCamera) return;
  cache_->coralCamera = position;
  cache_->coralCameraValid = true;
  auto& groups = cache_->coralGroups;
  for (auto& type : groups) {
    for (auto& variant : type) {
      for (auto& lod : variant) lod.clear();
    }
  }
  const std::uint64_t coralSeed = generator_.seeds().coralSeed;

  for (const std::uint64_t key : cache_->activeKeys) {
    const world::World11DecorChunk& chunk = cache_->chunks.at(key);
    for (const world::World11DecorInstance& instance : chunk.coral) {
      const float deltaX = instance.x - camera.position[0];
      const float deltaY = instance.y + 0.5f * instance.height -
          camera.position[1];
      const float deltaZ = instance.z - camera.position[2];
      const float distanceToCamera = std::sqrt(
          deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
      auto state = cache_->coralLodState.emplace(instance.stableId, -1).first;
      const CoralLodSelection selection = selectCoralLods(
          distanceToCamera, state->second);
      const std::size_t type = static_cast<std::size_t>(instance.variant) %
          CoralMeshSet::kTypeCount;
      const std::size_t meshVariant = coralMeshVariantIndex(
          instance, coralSeed);
      for (std::size_t draw = 0; draw < selection.count; ++draw) {
        groups[type][meshVariant][static_cast<std::size_t>(
            selection.lod[draw])].push_back(makeCoralGpuInstance(
                instance, selection.dither[draw]));
      }
    }
  }

  for (std::size_t type = 0; type < CoralMeshSet::kTypeCount; ++type) {
    for (std::size_t variant = 0;
         variant < CoralMeshSet::kVariantsPerType; ++variant) {
      for (std::size_t lod = 0; lod < CoralMeshSet::kLodCount; ++lod) {
        resources_->coralMeshes[type][variant][lod].uploadInstances(
            groups[type][variant][lod]);
      }
    }
  }
}

}  // namespace hg::render::gl33
