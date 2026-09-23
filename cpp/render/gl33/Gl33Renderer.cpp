#include "render/gl33/Gl33Renderer.hpp"

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Mesh.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33ShadowMap.hpp"
#include "render/gl33/World11WaterWaves.hpp"
#include "world/World11Seabed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kOceanFarPlane = 640.0f;
constexpr float kClipmapSnapSize = 12.0f;

struct GridLevelSpec {
  float outerExtent;
  float cellSize;
  float innerGeometryExtent;
  float innerBoundaryCellSize;
};

constexpr std::array<GridLevelSpec, 3> kGridLevels = {{
    {108.0f, 1.5f, 0.0f, 0.0f},
    {324.0f, 3.0f, 108.0f, 1.5f},
    {780.0f, 12.0f, 324.0f, 3.0f},
}};

static_assert(kGridLevels[2].outerExtent > kOceanFarPlane,
              "The far ocean edge must remain beyond the camera far plane");
static_assert(kGridLevels[0].outerExtent == kGridLevels[1].innerGeometryExtent &&
              kGridLevels[1].outerExtent == kGridLevels[2].innerGeometryExtent,
              "Adjacent ocean LOD levels must share exact boundaries");

struct Vec3 {
  float x;
  float y;
  float z;
};

using Matrix4 = std::array<float, 16>;

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
  return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return Vec3{
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

Vec3 normalize(const Vec3& value) {
  const float length = std::sqrt(dot(value, value));
  if (length <= 1.0e-6f) {
    return Vec3{0.0f, 1.0f, 0.0f};
  }
  return Vec3{value.x / length, value.y / length, value.z / length};
}

Matrix4 identityMatrix() {
  return Matrix4{
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
}

Matrix4 perspectiveMatrix(float verticalFovRadians, float aspect,
                          float nearPlane, float farPlane) {
  const float inverseTangent = 1.0f / std::tan(verticalFovRadians * 0.5f);
  Matrix4 matrix{};
  matrix[0] = inverseTangent / aspect;
  matrix[5] = inverseTangent;
  matrix[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
  matrix[11] = -1.0f;
  matrix[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
  return matrix;
}

Matrix4 viewMatrix(const Vec3& eye, const Vec3& front, const Vec3& cameraUp) {
  const Vec3 forward = normalize(front);
  const Vec3 right = normalize(cross(forward, cameraUp));
  const Vec3 up = normalize(cross(right, forward));
  return Matrix4{
      right.x, up.x, -forward.x, 0.0f,
      right.y, up.y, -forward.y, 0.0f,
      right.z, up.z, -forward.z, 0.0f,
      -dot(right, eye), -dot(up, eye), dot(forward, eye), 1.0f,
  };
}

void buildGridLevel(const GridLevelSpec& level,
                    std::vector<Gl33Vertex>& vertices,
                    std::vector<std::uint32_t>& indices) {
  const int segments = static_cast<int>(std::lround(
      (2.0f * level.outerExtent) / level.cellSize));
  const int rowSize = segments + 1;
  vertices.reserve(static_cast<std::size_t>(rowSize * rowSize));
  indices.reserve(static_cast<std::size_t>(segments * segments * 6));
  for (int row = 0; row <= segments; ++row) {
    const float z = -level.outerExtent + level.cellSize * static_cast<float>(row);
    for (int column = 0; column <= segments; ++column) {
      const float x = -level.outerExtent + level.cellSize * static_cast<float>(column);
      vertices.push_back(Gl33Vertex{{x, 0.0f, z}, {0.0f, 1.0f, 0.0f}});
    }
  }

  const auto addBoundaryVertex = [&vertices](float x, float z) {
    const std::uint32_t index = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back(Gl33Vertex{{x, 0.0f, z}, {0.0f, 1.0f, 0.0f}});
    return index;
  };
  const int stitchRatio = level.innerBoundaryCellSize > 0.0f
      ? static_cast<int>(std::lround(level.cellSize / level.innerBoundaryCellSize))
      : 1;

  for (int row = 0; row < segments; ++row) {
    for (int column = 0; column < segments; ++column) {
      const float x0 = -level.outerExtent + level.cellSize * static_cast<float>(column);
      const float x1 = x0 + level.cellSize;
      const float z0 = -level.outerExtent + level.cellSize * static_cast<float>(row);
      const float z1 = z0 + level.cellSize;
      const float centerX = (x0 + x1) * 0.5f;
      const float centerZ = (z0 + z1) * 0.5f;
      if (level.innerGeometryExtent > 0.0f &&
          std::max(std::abs(centerX), std::abs(centerZ)) < level.innerGeometryExtent) {
        continue;
      }
      const std::uint32_t topLeft = static_cast<std::uint32_t>(row * rowSize + column);
      const std::uint32_t topRight = topLeft + 1;
      const std::uint32_t bottomLeft = topLeft + static_cast<std::uint32_t>(rowSize);
      const std::uint32_t bottomRight = bottomLeft + 1;

      // The first coarse row around the inner hole is stitched to the exact
      // vertex spacing of the preceding LOD. This keeps the rings disjoint
      // while giving both meshes an identical shared boundary.
      const float inner = level.innerGeometryExtent;
      const bool alongHorizontalInnerEdge = x0 >= -inner && x1 <= inner;
      const bool alongVerticalInnerEdge = z0 >= -inner && z1 <= inner;
      const bool touchesBottom = stitchRatio > 1 && alongHorizontalInnerEdge &&
          std::abs(z1 + inner) < 0.001f;
      const bool touchesTop = stitchRatio > 1 && alongHorizontalInnerEdge &&
          std::abs(z0 - inner) < 0.001f;
      const bool touchesLeft = stitchRatio > 1 && alongVerticalInnerEdge &&
          std::abs(x1 + inner) < 0.001f;
      const bool touchesRight = stitchRatio > 1 && alongVerticalInnerEdge &&
          std::abs(x0 - inner) < 0.001f;

      if (touchesBottom || touchesTop) {
        std::vector<std::uint32_t> boundary;
        boundary.reserve(static_cast<std::size_t>(stitchRatio + 1));
        boundary.push_back(touchesBottom ? bottomLeft : topLeft);
        for (int step = 1; step < stitchRatio; ++step) {
          boundary.push_back(addBoundaryVertex(
              x0 + level.innerBoundaryCellSize * static_cast<float>(step),
              touchesBottom ? z1 : z0));
        }
        boundary.push_back(touchesBottom ? bottomRight : topRight);
        if (touchesBottom) {
          for (int step = 0; step < stitchRatio; ++step) {
            indices.insert(indices.end(),
                           {topLeft, boundary[step], boundary[step + 1]});
          }
          indices.insert(indices.end(), {topLeft, boundary.back(), topRight});
        } else {
          for (int step = 0; step < stitchRatio; ++step) {
            indices.insert(indices.end(),
                           {boundary[step], bottomLeft, boundary[step + 1]});
          }
          indices.insert(indices.end(), {boundary.back(), bottomLeft, bottomRight});
        }
        continue;
      }

      if (touchesLeft || touchesRight) {
        std::vector<std::uint32_t> boundary;
        boundary.reserve(static_cast<std::size_t>(stitchRatio + 1));
        boundary.push_back(touchesLeft ? topRight : topLeft);
        for (int step = 1; step < stitchRatio; ++step) {
          boundary.push_back(addBoundaryVertex(
              touchesLeft ? x1 : x0,
              z0 + level.innerBoundaryCellSize * static_cast<float>(step)));
        }
        boundary.push_back(touchesLeft ? bottomRight : bottomLeft);
        if (touchesLeft) {
          indices.insert(indices.end(), {topLeft, bottomLeft, boundary.back()});
          for (int step = 0; step < stitchRatio; ++step) {
            indices.insert(indices.end(),
                           {topLeft, boundary[step + 1], boundary[step]});
          }
        } else {
          for (int step = 0; step < stitchRatio; ++step) {
            indices.insert(indices.end(),
                           {boundary[step], boundary[step + 1], topRight});
          }
          indices.insert(indices.end(), {boundary.back(), bottomRight, topRight});
        }
        continue;
      }

      indices.push_back(topLeft);
      indices.push_back(bottomLeft);
      indices.push_back(topRight);
      indices.push_back(topRight);
      indices.push_back(bottomLeft);
      indices.push_back(bottomRight);
    }
  }
}

constexpr char kTerrainVertexShaderPrefix[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uPatchOrigin;

out vec3 vWorldPosition;
out vec3 vNormal;
)glsl";

constexpr char kTerrainVertexShaderSuffix[] = R"glsl(
void main() {
  vec2 point = aPosition.xz + uPatchOrigin.xz;
  const float derivativeStep = 0.16;
  float height = seabedHeight(point);
  float slopeX = seabedHeight(point + vec2(derivativeStep, 0.0)) -
                 seabedHeight(point - vec2(derivativeStep, 0.0));
  float slopeZ = seabedHeight(point + vec2(0.0, derivativeStep)) -
                 seabedHeight(point - vec2(0.0, derivativeStep));
  vec3 localNormal = normalize(vec3(-slopeX, derivativeStep * 2.0, -slopeZ));
  vec4 worldPosition = uModel * vec4(point.x, height, point.y, 1.0);
  vWorldPosition = worldPosition.xyz;
  vNormal = normalize(mat3(uModel) * localNormal);
  gl_Position = uProjection * uView * worldPosition;
}
)glsl";

std::string terrainVertexShaderSource() {
  std::string source{kTerrainVertexShaderPrefix};
  source += hg::world::kWorld11SeabedGlsl;
  source += kTerrainVertexShaderSuffix;
  return source;
}

constexpr char kWaterVertexShaderPrefix[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform vec3 uPatchOrigin;

out vec3 vWorldPosition;
out vec3 vNormal;
out float vSurfaceViewDepth;
)glsl";

constexpr char kWaterVertexShaderSuffix[] = R"glsl(
void main() {
  vec2 point = aPosition.xz + uPatchOrigin.xz;
  vec3 position = world11WaterDisplacedPosition(point);
  const float derivativeStep = 0.12;
  vec3 tangentX = world11WaterDisplacedPosition(
      point + vec2(derivativeStep, 0.0)) -
      world11WaterDisplacedPosition(point - vec2(derivativeStep, 0.0));
  vec3 tangentZ = world11WaterDisplacedPosition(
      point + vec2(0.0, derivativeStep)) -
      world11WaterDisplacedPosition(point - vec2(0.0, derivativeStep));
  vec4 worldPosition = uModel * vec4(position, 1.0);
  vWorldPosition = worldPosition.xyz;
  vNormal = normalize(mat3(uModel) * cross(tangentZ, tangentX));
  vSurfaceViewDepth = -(uView * worldPosition).z;
  gl_Position = uProjection * uView * worldPosition;
}
)glsl";

std::string waterVertexShaderSource() {
  std::string source{kWaterVertexShaderPrefix};
  source += kWorld11WaterWavesGlsl;
  source += kWaterVertexShaderSuffix;
  return source;
}

constexpr char kTerrainFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;

uniform vec4 uBaseColor;
uniform vec3 uLightDirection;
uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;

out vec4 fragmentColor;

void main() {
  vec3 normal = normalize(vNormal);
  float diffuse = max(dot(normal, normalize(-uLightDirection)), 0.0);
  float shadow = world11ShadowVisibility(
      vWorldPosition, normal, uLightDirection);
  float light = 0.28 + diffuse * 0.72 * shadow;
  vec3 litColor = uBaseColor.rgb * light;
  float distanceToCamera = length(vWorldPosition - uCameraPosition);
  float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
  float fragmentDepth = max(0.0, uWaterSurfaceY - vWorldPosition.y);
  float meanDepth = min(24.0, 0.5 * (cameraDepth + fragmentDepth));
  float density = uAbsorptionDensity + uDepthAbsorption * meanDepth;
  float transmittance = exp(-density * distanceToCamera);
  float fog = 1.0 - transmittance;
  if (cameraDepth > 0.0) {
    vec3 viewRay = (vWorldPosition - uCameraPosition) /
        max(distanceToCamera, 0.0001);
    float grazingRay = 1.0 - smoothstep(0.035, 0.14, abs(viewRay.y));
    float grazingScattering = grazingRay * smoothstep(4.0, 28.0, distanceToCamera);
    fog = max(fog, grazingScattering);
  }
  fragmentColor = vec4(mix(litColor, uFogColor, fog), uBaseColor.a);
}
)glsl";

constexpr char kFullscreenVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
out vec2 vUv;
void main() {
  vUv = aPosition.xy * 0.5 + 0.5;
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
)glsl";

constexpr char kSceneCopyFragmentShader[] = R"glsl(#version 330 core
in vec2 vUv;
uniform sampler2D uSceneColor;
out vec4 fragmentColor;
void main() {
  fragmentColor = texture(uSceneColor, vUv);
}
)glsl";

constexpr char kWaterCompositeFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in float vSurfaceViewDepth;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform vec3 uCameraPosition;
uniform vec3 uAbsorptionCoefficient;
uniform vec3 uWaterScatterColor;
uniform vec3 uReflectionColor;
uniform float uNearPlane;
uniform float uFarPlane;
uniform float uWaterSurfaceY;

out vec4 fragmentColor;

float linearViewDepth(float depthSample) {
  float ndcDepth = depthSample * 2.0 - 1.0;
  return (2.0 * uNearPlane * uFarPlane) /
      (uFarPlane + uNearPlane - ndcDepth * (uFarPlane - uNearPlane));
}

void main() {
  ivec2 targetSize = textureSize(uSceneColor, 0);
  vec2 inverseSize = 1.0 / vec2(targetSize);
  vec2 screenUv = gl_FragCoord.xy * inverseSize;
  float centerDepthSample = texture(uSceneDepth, screenUv).r;
  float centerSceneDepth = linearViewDepth(centerDepthSample);
  if (centerDepthSample < 0.99999 &&
      centerSceneDepth < vSurfaceViewDepth - 0.025) {
    discard;
  }

  vec3 surfaceToCamera = normalize(uCameraPosition - vWorldPosition);
  vec3 geometricNormal = normalize(vNormal);
  vec3 normal = faceforward(geometricNormal,
                            -surfaceToCamera,
                            geometricNormal);
  float cosView = clamp(dot(normal, surfaceToCamera), 0.0, 1.0);
  float fresnel = 0.02 + 0.98 * pow(1.0 - cosView, 5.0);
  bool underwater = uCameraPosition.y < uWaterSurfaceY;

  float preliminaryThickness = underwater
      ? max(vSurfaceViewDepth, 0.0)
      : max(centerSceneDepth - vSurfaceViewDepth, 0.0);
  float refractionStrength = (underwater ? 0.006 : 0.012) *
      clamp(preliminaryThickness / 12.0, 0.0, 1.0) * (1.0 - fresnel);
  vec2 refractedUv = clamp(screenUv + normal.xz * refractionStrength,
                           inverseSize * 1.5, vec2(1.0) - inverseSize * 1.5);

  float refractedDepthSample = texture(uSceneDepth, refractedUv).r;
  float refractedSceneDepth = linearViewDepth(refractedDepthSample);
  if (refractedDepthSample < 0.99999 &&
      refractedSceneDepth < vSurfaceViewDepth) {
    refractedUv = screenUv;
    refractedSceneDepth = centerSceneDepth;
  }

  float waterThickness = underwater
      ? max(vSurfaceViewDepth, 0.0)
      : max(refractedSceneDepth - vSurfaceViewDepth, 0.0);
  waterThickness = clamp(waterThickness, 0.0, 80.0);
  vec3 transmittance = exp(-uAbsorptionCoefficient * waterThickness);
  vec3 refractedColor = texture(uSceneColor, refractedUv).rgb;
  vec3 transmittedColor = refractedColor * transmittance +
      uWaterScatterColor * (vec3(1.0) - transmittance);

  vec3 lightDirection = normalize(vec3(0.32, 0.88, 0.36));
  vec3 halfVector = normalize(lightDirection + surfaceToCamera);
  float sunGlint = pow(max(abs(dot(normal, halfVector)), 0.0), 96.0) * 0.55;
  vec3 reflectedColor = (underwater
      ? mix(uWaterScatterColor, uReflectionColor, 0.35)
      : uReflectionColor) + vec3(sunGlint);
  fragmentColor = vec4(mix(transmittedColor, reflectedColor, fresnel), 1.0);
}
)glsl";

constexpr char kDepthOnlyFragmentShader[] = R"glsl(#version 330 core
void main() {
}
)glsl";

void setSharedUniforms(Gl33ShaderProgram& program, const Matrix4& model,
                       const Matrix4& view, const Matrix4& projection,
                       const Vec3& camera, const Vec3& patchOrigin) {
  program.setMatrix4("uModel", model.data());
  program.setMatrix4("uView", view.data());
  program.setMatrix4("uProjection", projection.data());
  program.setVec3("uLightDirection", -0.32f, -0.88f, -0.36f);
  program.setVec3("uCameraPosition", camera.x, camera.y, camera.z);
  program.setVec3("uPatchOrigin", patchOrigin.x, 0.0f, patchOrigin.z);
  program.setVec3("uFogColor", 0.10f, 0.32f, 0.40f);
  program.setFloat("uWaterSurfaceY", 12.5f);
  program.setFloat("uAbsorptionDensity", 0.008f);
  program.setFloat("uDepthAbsorption", 0.0017f);
}

void buildFullscreenQuad(Gl33Mesh& mesh) {
  const std::vector<Gl33Vertex> vertices = {
      {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
  };
  const std::vector<std::uint32_t> indices = {0, 1, 2, 0, 2, 3};
  mesh.upload(vertices, indices);
}

}  // namespace

struct Gl33Renderer::Resources {
  ~Resources() {
    Gl33Api& gl = api();
    if (sceneColorTexture != 0) {
      gl.DeleteTextures(1, &sceneColorTexture);
    }
    if (sceneDepthTexture != 0) {
      gl.DeleteTextures(1, &sceneDepthTexture);
    }
    if (waterSurfaceDepthTexture != 0) {
      gl.DeleteTextures(1, &waterSurfaceDepthTexture);
    }
    if (sceneFramebuffer != 0) {
      gl.DeleteFramebuffers(1, &sceneFramebuffer);
    }
  }

  std::array<Gl33Mesh, 3> terrainMeshes;
  std::array<Gl33Mesh, 3> waterMeshes;
  Gl33Mesh fullscreenQuad;
  Gl33ShaderProgram terrainProgram;
  Gl33ShaderProgram waterProgram;
  Gl33ShaderProgram waterDepthProgram;
  Gl33ShaderProgram sceneCopyProgram;
  UInt sceneFramebuffer = 0;
  UInt sceneColorTexture = 0;
  UInt sceneDepthTexture = 0;
  UInt waterSurfaceDepthTexture = 0;
  int targetWidth = 0;
  int targetHeight = 0;
};

Gl33Renderer::Gl33Renderer() = default;

Gl33Renderer::~Gl33Renderer() = default;

void Gl33Renderer::beginOpaquePass(const Gl33Camera& camera) {
  ensureInitialized();
  const int width = std::max(camera.framebufferWidth, 1);
  const int height = std::max(camera.framebufferHeight, 1);
  ensureSceneTarget(width, height);
  Gl33Api& gl = api();
  gl.BindFramebuffer(kFramebuffer, resources_->sceneFramebuffer);
  gl.Viewport(0, 0, width, height);
  gl.ClearColor(0.10f, 0.32f, 0.40f, 1.0f);
  gl.Clear(kColorBufferBit | kDepthBufferBit);
  gl.Enable(kDepthTest);
  gl.Disable(kBlend);
  gl.DepthMask(kTrue);
}

void Gl33Renderer::renderSeabed(const Gl33Camera& camera,
                                const Gl33ShadowMap* shadowMap) {
  ensureInitialized();
  const Vec3 position{camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const float aspect = static_cast<float>(std::max(camera.framebufferWidth, 1)) /
      static_cast<float>(std::max(camera.framebufferHeight, 1));
  const Matrix4 model = identityMatrix();
  const Matrix4 view = viewMatrix(position, front, up);
  const Matrix4 projection = perspectiveMatrix(
      60.0f * kPi / 180.0f, aspect, 0.1f, kOceanFarPlane);
  const Vec3 patchOrigin{
      std::round(position.x / kClipmapSnapSize) * kClipmapSnapSize,
      0.0f,
      std::round(position.z / kClipmapSnapSize) * kClipmapSnapSize,
  };
  Gl33Api& gl = api();

  gl.Disable(kBlend);
  gl.DepthMask(kTrue);
  for (std::size_t levelIndex = kGridLevels.size(); levelIndex-- > 0;) {
    resources_->terrainProgram.use();
    setSharedUniforms(resources_->terrainProgram, model, view, projection,
                      position, patchOrigin);
    resources_->terrainProgram.setVec4("uBaseColor", 0.52f, 0.40f, 0.22f, 1.0f);
    if (shadowMap != nullptr) {
      shadowMap->applyToReceiver(resources_->terrainProgram);
    } else {
      resources_->terrainProgram.setInt("uShadowEnabled", 0);
    }
    resources_->terrainMeshes[levelIndex].draw();
  }
  gl.UseProgram(0);
}

void Gl33Renderer::renderWaterSurfaceDepth(
    const Gl33Camera& camera, float time) {
  ensureInitialized();
  const Vec3 position{camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const int width = std::max(camera.framebufferWidth, 1);
  const int height = std::max(camera.framebufferHeight, 1);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const Matrix4 model = identityMatrix();
  const Matrix4 view = viewMatrix(position, front, up);
  const Matrix4 projection = perspectiveMatrix(
      60.0f * kPi / 180.0f, aspect, 0.1f, kOceanFarPlane);
  const Vec3 patchOrigin{
      std::round(position.x / kClipmapSnapSize) * kClipmapSnapSize,
      0.0f,
      std::round(position.z / kClipmapSnapSize) * kClipmapSnapSize,
  };

  ensureSceneTarget(width, height);
  Gl33Api& gl = api();
  gl.BindFramebuffer(kFramebuffer, resources_->sceneFramebuffer);
  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment, kTexture2D,
                          resources_->waterSurfaceDepthTexture, 0);
  gl.Viewport(0, 0, width, height);
  gl.DepthMask(kTrue);
  gl.Clear(kDepthBufferBit);
  gl.Enable(kDepthTest);
  gl.DepthFunc(kLessEqual);
  gl.Disable(kBlend);
  gl.Disable(kCullFace);

  for (std::size_t levelIndex = kGridLevels.size(); levelIndex-- > 0;) {
    resources_->waterDepthProgram.use();
    resources_->waterDepthProgram.setMatrix4("uModel", model.data());
    resources_->waterDepthProgram.setMatrix4("uView", view.data());
    resources_->waterDepthProgram.setMatrix4("uProjection", projection.data());
    resources_->waterDepthProgram.setVec3(
        "uPatchOrigin", patchOrigin.x, 0.0f, patchOrigin.z);
    resources_->waterDepthProgram.setFloat("uTime", time);
    resources_->waterMeshes[levelIndex].draw();
  }

  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment, kTexture2D,
                          resources_->sceneDepthTexture, 0);
  gl.UseProgram(0);
}

std::uint32_t Gl33Renderer::waterSurfaceDepthTexture() const {
  return resources_ == nullptr ? 0U : resources_->waterSurfaceDepthTexture;
}

void Gl33Renderer::composeWater(const Gl33Camera& camera, float time) {
  ensureInitialized();
  const Vec3 position{camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const int width = std::max(camera.framebufferWidth, 1);
  const int height = std::max(camera.framebufferHeight, 1);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const Matrix4 model = identityMatrix();
  const Matrix4 view = viewMatrix(position, front, up);
  const Matrix4 projection = perspectiveMatrix(
      60.0f * kPi / 180.0f, aspect, 0.1f, kOceanFarPlane);
  const Vec3 patchOrigin{
      std::round(position.x / kClipmapSnapSize) * kClipmapSnapSize,
      0.0f,
      std::round(position.z / kClipmapSnapSize) * kClipmapSnapSize,
  };
  Gl33Api& gl = api();

  gl.BindFramebuffer(kReadFramebuffer, resources_->sceneFramebuffer);
  gl.BindFramebuffer(kDrawFramebuffer, 0);
  gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                     kDepthBufferBit, kNearest);
  gl.BindFramebuffer(kFramebuffer, 0);
  gl.Viewport(0, 0, width, height);
  gl.Disable(kDepthTest);
  gl.Disable(kBlend);
  gl.DepthMask(kFalse);

  resources_->sceneCopyProgram.use();
  resources_->sceneCopyProgram.setInt("uSceneColor", 0);
  gl.ActiveTexture(kTexture0);
  gl.BindTexture(kTexture2D, resources_->sceneColorTexture);
  resources_->fullscreenQuad.draw();

  gl.Enable(kDepthTest);
  gl.DepthFunc(kLessEqual);
  gl.DepthMask(kTrue);
  gl.Disable(kCullFace);

  for (std::size_t levelIndex = kGridLevels.size(); levelIndex-- > 0;) {
    resources_->waterProgram.use();
    resources_->waterProgram.setMatrix4("uModel", model.data());
    resources_->waterProgram.setMatrix4("uView", view.data());
    resources_->waterProgram.setMatrix4("uProjection", projection.data());
    resources_->waterProgram.setVec3("uCameraPosition",
                                     position.x, position.y, position.z);
    resources_->waterProgram.setVec3("uPatchOrigin",
                                     patchOrigin.x, 0.0f, patchOrigin.z);
    resources_->waterProgram.setFloat("uTime", time);
    resources_->waterProgram.setInt("uSceneColor", 0);
    resources_->waterProgram.setInt("uSceneDepth", 1);
    resources_->waterProgram.setVec3("uAbsorptionCoefficient",
                                     0.20f, 0.065f, 0.028f);
    resources_->waterProgram.setVec3("uWaterScatterColor",
                                     0.018f, 0.19f, 0.27f);
    resources_->waterProgram.setVec3("uReflectionColor",
                                     0.36f, 0.68f, 0.82f);
    resources_->waterProgram.setFloat("uNearPlane", 0.1f);
    resources_->waterProgram.setFloat("uFarPlane", kOceanFarPlane);
    resources_->waterProgram.setFloat("uWaterSurfaceY", 12.5f);
    gl.ActiveTexture(kTexture0);
    gl.BindTexture(kTexture2D, resources_->sceneColorTexture);
    gl.ActiveTexture(kTexture0 + 1);
    gl.BindTexture(kTexture2D, resources_->sceneDepthTexture);
    resources_->waterMeshes[levelIndex].draw();
  }

  gl.ActiveTexture(kTexture0 + 1);
  gl.BindTexture(kTexture2D, 0);
  gl.ActiveTexture(kTexture0);
  gl.BindTexture(kTexture2D, 0);
  gl.DepthMask(kTrue);
  gl.Enable(kDepthTest);
  gl.UseProgram(0);
}

void Gl33Renderer::ensureSceneTarget(int width, int height) {
  if (resources_->targetWidth == width && resources_->targetHeight == height) {
    return;
  }

  Gl33Api& gl = api();
  if (resources_->sceneFramebuffer == 0) {
    gl.GenFramebuffers(1, &resources_->sceneFramebuffer);
    gl.GenTextures(1, &resources_->sceneColorTexture);
    gl.GenTextures(1, &resources_->sceneDepthTexture);
    gl.GenTextures(1, &resources_->waterSurfaceDepthTexture);
  }

  gl.BindTexture(kTexture2D, resources_->sceneColorTexture);
  gl.TexParameteri(kTexture2D, kTextureMinFilter, static_cast<Int>(kLinear));
  gl.TexParameteri(kTexture2D, kTextureMagFilter, static_cast<Int>(kLinear));
  gl.TexParameteri(kTexture2D, kTextureWrapS, static_cast<Int>(kClampToEdge));
  gl.TexParameteri(kTexture2D, kTextureWrapT, static_cast<Int>(kClampToEdge));
  gl.TexImage2D(kTexture2D, 0, kRgba8, width, height, 0,
                kRgba, kUnsignedByte, nullptr);

  gl.BindTexture(kTexture2D, resources_->sceneDepthTexture);
  gl.TexParameteri(kTexture2D, kTextureMinFilter, static_cast<Int>(kNearest));
  gl.TexParameteri(kTexture2D, kTextureMagFilter, static_cast<Int>(kNearest));
  gl.TexParameteri(kTexture2D, kTextureWrapS, static_cast<Int>(kClampToEdge));
  gl.TexParameteri(kTexture2D, kTextureWrapT, static_cast<Int>(kClampToEdge));
  gl.TexImage2D(kTexture2D, 0, kDepthComponent24, width, height, 0,
                 kDepthComponent, kUnsignedInt, nullptr);

  gl.BindTexture(kTexture2D, resources_->waterSurfaceDepthTexture);
  gl.TexParameteri(kTexture2D, kTextureMinFilter, static_cast<Int>(kNearest));
  gl.TexParameteri(kTexture2D, kTextureMagFilter, static_cast<Int>(kNearest));
  gl.TexParameteri(kTexture2D, kTextureWrapS, static_cast<Int>(kClampToEdge));
  gl.TexParameteri(kTexture2D, kTextureWrapT, static_cast<Int>(kClampToEdge));
  gl.TexImage2D(kTexture2D, 0, kDepthComponent24, width, height, 0,
                kDepthComponent, kUnsignedInt, nullptr);
  gl.BindTexture(kTexture2D, 0);

  gl.BindFramebuffer(kFramebuffer, resources_->sceneFramebuffer);
  gl.FramebufferTexture2D(kFramebuffer, kColorAttachment0,
                          kTexture2D, resources_->sceneColorTexture, 0);
  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment,
                          kTexture2D, resources_->sceneDepthTexture, 0);
  const Enum sceneStatus = gl.CheckFramebufferStatus(kFramebuffer);
  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment,
                          kTexture2D, resources_->waterSurfaceDepthTexture, 0);
  const Enum waterDepthStatus = gl.CheckFramebufferStatus(kFramebuffer);
  gl.FramebufferTexture2D(kFramebuffer, kDepthAttachment,
                          kTexture2D, resources_->sceneDepthTexture, 0);
  gl.BindFramebuffer(kFramebuffer, 0);
  if (sceneStatus != kFramebufferComplete) {
    throw std::runtime_error("World 11 scene framebuffer is incomplete");
  }
  if (waterDepthStatus != kFramebufferComplete) {
    throw std::runtime_error(
        "World 11 water depth framebuffer is incomplete");
  }
  resources_->targetWidth = width;
  resources_->targetHeight = height;
}

void Gl33Renderer::ensureInitialized() {
  if (resources_ != nullptr) {
    return;
  }
  resources_ = std::make_unique<Resources>();
  const std::string terrainShader = terrainVertexShaderSource();
  const std::string terrainFragmentShader =
      withWorld11Shadows(kTerrainFragmentShader);
  resources_->terrainProgram.build(terrainShader.c_str(),
                                   terrainFragmentShader.c_str());
  initializeWorld11ShadowReceiver(resources_->terrainProgram);
  const std::string waterShader = waterVertexShaderSource();
  resources_->waterProgram.build(
      waterShader.c_str(), kWaterCompositeFragmentShader);
  resources_->waterDepthProgram.build(
      waterShader.c_str(), kDepthOnlyFragmentShader);
  resources_->sceneCopyProgram.build(kFullscreenVertexShader,
                                     kSceneCopyFragmentShader);
  buildFullscreenQuad(resources_->fullscreenQuad);

  for (std::size_t levelIndex = 0; levelIndex < kGridLevels.size(); ++levelIndex) {
    std::vector<Gl33Vertex> vertices;
    std::vector<std::uint32_t> indices;
    buildGridLevel(kGridLevels[levelIndex], vertices, indices);
    resources_->terrainMeshes[levelIndex].upload(vertices, indices);
    resources_->waterMeshes[levelIndex].upload(vertices, indices);
  }
}

}  // namespace hg::render::gl33
