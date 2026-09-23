#include "render/gl33/Gl33WorldRenderer.hpp"

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Mesh.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33Skybox.hpp"
#include "render/gl33/Gl33Texture.hpp"
#include "render/gl33/Gl33World11DecorRenderer.hpp"
#include "render/gl33/Gl33World11FishRenderer.hpp"
#include "world/PortalController.hpp"
#include "world/World11Seabed.hpp"
#include "world/World11Landmarks.hpp"
#include "render/gl33/World11Environment.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Color {
  float red;
  float green;
  float blue;
  float alpha = 1.0f;
};

using Matrix4 = std::array<float, 16>;

struct MeshData {
  std::vector<Gl33Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

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
  if (length < 1.0e-6f) {
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

Matrix4 translation(float x, float y, float z) {
  Matrix4 matrix = identityMatrix();
  matrix[12] = x;
  matrix[13] = y;
  matrix[14] = z;
  return matrix;
}

Matrix4 scale(float x, float y, float z) {
  Matrix4 matrix{};
  matrix[0] = x;
  matrix[5] = y;
  matrix[10] = z;
  matrix[15] = 1.0f;
  return matrix;
}

Matrix4 rotationX(float angle) {
  Matrix4 matrix = identityMatrix();
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  matrix[5] = cosine;
  matrix[6] = sine;
  matrix[9] = -sine;
  matrix[10] = cosine;
  return matrix;
}

Matrix4 rotationY(float angle) {
  Matrix4 matrix = identityMatrix();
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  matrix[0] = cosine;
  matrix[2] = -sine;
  matrix[8] = sine;
  matrix[10] = cosine;
  return matrix;
}

Matrix4 rotationZ(float angle) {
  Matrix4 matrix = identityMatrix();
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  matrix[0] = cosine;
  matrix[1] = sine;
  matrix[4] = -sine;
  matrix[5] = cosine;
  return matrix;
}

Matrix4 modelMatrix(float x, float y, float z,
                    float scaleX, float scaleY, float scaleZ,
                    float rotateX = 0.0f, float rotateY = 0.0f,
                    float rotateZ = 0.0f) {
  Matrix4 rotation = multiply(rotationY(rotateY), rotationX(rotateX));
  rotation = multiply(rotation, rotationZ(rotateZ));
  return multiply(translation(x, y, z), multiply(rotation, scale(scaleX, scaleY, scaleZ)));
}

Matrix4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
  const float inverseTangent = 1.0f / std::tan(fovRadians * 0.5f);
  Matrix4 matrix{};
  matrix[0] = inverseTangent / aspect;
  matrix[5] = inverseTangent;
  matrix[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
  matrix[11] = -1.0f;
  matrix[14] = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
  return matrix;
}

Matrix4 view(const Vec3& eye, const Vec3& front, const Vec3& cameraUp) {
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

void addTriangle(MeshData& mesh, const Vec3& a, const Vec3& b, const Vec3& c) {
  const Vec3 normal = normalize(cross(subtract(b, a), subtract(c, a)));
  const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(Gl33Vertex{{a.x, a.y, a.z}, {normal.x, normal.y, normal.z}});
  mesh.vertices.push_back(Gl33Vertex{{b.x, b.y, b.z}, {normal.x, normal.y, normal.z}});
  mesh.vertices.push_back(Gl33Vertex{{c.x, c.y, c.z}, {normal.x, normal.y, normal.z}});
  mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}

void addQuad(MeshData& mesh, const Vec3& a, const Vec3& b,
             const Vec3& c, const Vec3& d) {
  const std::size_t first = mesh.vertices.size();
  addTriangle(mesh, a, b, c);
  addTriangle(mesh, a, c, d);
  constexpr std::array<std::array<float, 2>, 6> uv = {{
      {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
      {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
  }};
  for (std::size_t index = 0; index < uv.size(); ++index) {
    mesh.vertices[first + index].uv[0] = uv[index][0];
    mesh.vertices[first + index].uv[1] = uv[index][1];
  }
}

MeshData makeCube() {
  MeshData mesh;
  addQuad(mesh, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
          {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f});
  addQuad(mesh, {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f},
          {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f});
  addQuad(mesh, {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f},
          {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f});
  addQuad(mesh, {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f},
          {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
  addQuad(mesh, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
          {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f});
  addQuad(mesh, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
          {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f});
  return mesh;
}

MeshData makePyramid() {
  MeshData mesh;
  const Vec3 a{-0.6f, 0.0f, -0.6f};
  const Vec3 b{0.6f, 0.0f, -0.6f};
  const Vec3 c{0.6f, 0.0f, 0.6f};
  const Vec3 d{-0.6f, 0.0f, 0.6f};
  const Vec3 top{0.0f, 1.5f, 0.0f};
  addQuad(mesh, a, d, c, b);
  addTriangle(mesh, top, a, b);
  addTriangle(mesh, top, b, c);
  addTriangle(mesh, top, c, d);
  addTriangle(mesh, top, d, a);
  return mesh;
}

MeshData makeCylinder(int sides) {
  MeshData mesh;
  for (int side = 0; side < sides; ++side) {
    const float a0 = kTwoPi * static_cast<float>(side) / static_cast<float>(sides);
    const float a1 = kTwoPi * static_cast<float>(side + 1) / static_cast<float>(sides);
    const Vec3 p0{std::cos(a0), 0.0f, std::sin(a0)};
    const Vec3 p1{std::cos(a1), 0.0f, std::sin(a1)};
    const Vec3 q0{p0.x, 1.0f, p0.z};
    const Vec3 q1{p1.x, 1.0f, p1.z};
    addQuad(mesh, p0, p1, q1, q0);
    addTriangle(mesh, {0.0f, 1.0f, 0.0f}, q0, q1);
    addTriangle(mesh, {0.0f, 0.0f, 0.0f}, p1, p0);
  }
  return mesh;
}

MeshData makeCrystal() {
  MeshData mesh;
  constexpr int kSides = 6;
  for (int side = 0; side < kSides; ++side) {
    const float a0 = kTwoPi * static_cast<float>(side) / static_cast<float>(kSides);
    const float a1 = kTwoPi * static_cast<float>(side + 1) / static_cast<float>(kSides);
    const Vec3 base0{std::cos(a0), 0.0f, std::sin(a0)};
    const Vec3 base1{std::cos(a1), 0.0f, std::sin(a1)};
    const Vec3 shoulder0{std::cos(a0) * 0.76f, 0.68f, std::sin(a0) * 0.76f};
    const Vec3 shoulder1{std::cos(a1) * 0.76f, 0.68f, std::sin(a1) * 0.76f};
    addQuad(mesh, base0, base1, shoulder1, shoulder0);
    addTriangle(mesh, shoulder0, shoulder1, {0.0f, 1.0f, 0.0f});
  }
  return mesh;
}

MeshData makeAnnulus() {
  MeshData mesh;
  constexpr int kSegments = 64;
  constexpr float kInner = 0.78f;
  for (int segment = 0; segment < kSegments; ++segment) {
    const float a0 = kTwoPi * static_cast<float>(segment) / static_cast<float>(kSegments);
    const float a1 = kTwoPi * static_cast<float>(segment + 1) / static_cast<float>(kSegments);
    addQuad(mesh,
        {std::cos(a0) * kInner, std::sin(a0) * kInner, 0.0f},
        {std::cos(a1) * kInner, std::sin(a1) * kInner, 0.0f},
        {std::cos(a1), std::sin(a1), 0.0f},
        {std::cos(a0), std::sin(a0), 0.0f});
  }
  return mesh;
}

MeshData makePortalQuad() {
  MeshData mesh;
  addQuad(mesh,
          {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
          {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f});
  return mesh;
}

MeshData makeSphere() {
  MeshData mesh;
  constexpr int kLatitude = 10;
  constexpr int kLongitude = 16;
  for (int latitude = 0; latitude < kLatitude; ++latitude) {
    const float p0 = -0.5f * kPi + kPi * static_cast<float>(latitude) /
        static_cast<float>(kLatitude);
    const float p1 = -0.5f * kPi + kPi * static_cast<float>(latitude + 1) /
        static_cast<float>(kLatitude);
    for (int longitude = 0; longitude < kLongitude; ++longitude) {
      const float t0 = kTwoPi * static_cast<float>(longitude) / static_cast<float>(kLongitude);
      const float t1 = kTwoPi * static_cast<float>(longitude + 1) / static_cast<float>(kLongitude);
      const Vec3 a{std::cos(p0) * std::cos(t0), std::sin(p0), std::cos(p0) * std::sin(t0)};
      const Vec3 b{std::cos(p0) * std::cos(t1), std::sin(p0), std::cos(p0) * std::sin(t1)};
      const Vec3 c{std::cos(p1) * std::cos(t1), std::sin(p1), std::cos(p1) * std::sin(t1)};
      const Vec3 d{std::cos(p1) * std::cos(t0), std::sin(p1), std::cos(p1) * std::sin(t0)};
      addQuad(mesh, a, b, c, d);
    }
  }
  return mesh;
}

MeshData makeReefRock() {
  MeshData mesh;

  // A low-poly chamfered rubble block reads more like masonry than the old
  // deformed sphere. Three irregular rings create broad planar faces while
  // preserving enough asymmetry for natural reef stone.
  constexpr int kSides = 8;
  constexpr std::array<std::array<float, 2>, kSides> outline = {{
      { 0.55f, -0.95f},
      { 0.95f, -0.55f},
      { 0.95f,  0.55f},
      { 0.55f,  0.95f},
      {-0.55f,  0.95f},
      {-0.95f,  0.55f},
      {-0.95f, -0.55f},
      {-0.55f, -0.95f},
  }};

  std::array<Vec3, kSides> lower{};
  std::array<Vec3, kSides> middle{};
  std::array<Vec3, kSides> upper{};

  for (int side = 0; side < kSides; ++side) {
    const float x = outline[static_cast<std::size_t>(side)][0];
    const float z = outline[static_cast<std::size_t>(side)][1];
    const float phase = static_cast<float>(side);

    const float lowerX = 0.91f + 0.055f * std::sin(phase * 2.17f + 0.4f);
    const float lowerZ = 0.93f + 0.050f * std::cos(phase * 1.73f + 0.9f);
    const float middleX = 1.00f + 0.060f * std::cos(phase * 2.41f + 0.2f);
    const float middleZ = 0.99f + 0.055f * std::sin(phase * 1.91f + 1.1f);
    const float upperX = 0.84f + 0.055f * std::sin(phase * 2.63f + 1.4f);
    const float upperZ = 0.87f + 0.050f * std::cos(phase * 2.07f + 0.5f);

    lower[static_cast<std::size_t>(side)] = {
        x * lowerX - 0.035f,
        -0.80f + 0.035f * std::sin(phase * 2.31f),
        z * lowerZ + 0.025f};

    middle[static_cast<std::size_t>(side)] = {
        x * middleX + 0.055f,
        -0.06f + 0.055f * std::cos(phase * 1.67f + 0.6f),
        z * middleZ - 0.035f};

    upper[static_cast<std::size_t>(side)] = {
        x * upperX - 0.015f,
        0.74f + 0.045f * std::sin(phase * 2.83f + 0.3f),
        z * upperZ + 0.045f};
  }

  for (int side = 0; side < kSides; ++side) {
    const int next = (side + 1) % kSides;
    const std::size_t i = static_cast<std::size_t>(side);
    const std::size_t j = static_cast<std::size_t>(next);

    // Reverse ring order here so the side normals point outwards.
    addQuad(mesh, lower[j], lower[i], middle[i], middle[j]);
    addQuad(mesh, middle[j], middle[i], upper[i], upper[j]);
  }

  const Vec3 bottomCenter{-0.03f, -0.82f, 0.02f};
  const Vec3 topCenter{0.01f, 0.77f, 0.03f};
  for (int side = 0; side < kSides; ++side) {
    const int next = (side + 1) % kSides;
    const std::size_t i = static_cast<std::size_t>(side);
    const std::size_t j = static_cast<std::size_t>(next);

    // Bottom faces downward, top faces upward.
    addTriangle(mesh, bottomCenter, lower[i], lower[j]);
    addTriangle(mesh, topCenter, upper[j], upper[i]);
  }

  return mesh;
}

float world7Height(float x, float z) {
  const float dx = x;
  const float dz = z + 50.0f;
  const float radius = std::sqrt(dx * dx + dz * dz);
  const float hill = std::max(0.0f, 1.0f - radius / 14.0f);
  const float noise = std::sin(x * 0.61f + z * 0.17f) * 0.18f * hill;
  return -1.0f + hill * hill * 9.5f + noise;
}

float world12Height(float x, float z) {
  const float smallRock =
      std::sin(x * 0.19f + z * 0.07f) * 0.24f +
      std::sin(-x * 0.08f + z * 0.17f + 1.3f) * 0.18f +
      std::sin(x * 0.42f - z * 0.31f + 0.7f) * 0.07f;
  const float broadRock = std::sin(x * 0.035f) * std::cos(z * 0.029f) * 0.48f;
  const float craterDistance = std::sqrt(x * x + (z + 112.0f) * (z + 112.0f));
  const float cone = std::max(0.0f, 1.0f - craterDistance / 80.0f) * 30.0f;
  const float rimOffset = (craterDistance - 18.0f) / 5.8f;
  const float rim = std::exp(-rimOffset * rimOffset) * 9.0f;
  const float craterRatio = craterDistance / 11.5f;
  const float bowl = std::exp(-craterRatio * craterRatio * craterRatio * craterRatio) * 4.0f;
  const float lakeX = (x - 24.0f) / 13.5f;
  const float lakeZ = (z + 36.0f) / 9.5f;
  const float lakeDistance = std::sqrt(lakeX * lakeX + lakeZ * lakeZ);
  const float basin = std::exp(-lakeDistance * lakeDistance * lakeDistance * lakeDistance) * 1.65f;
  return -1.0f + smallRock + broadRock + cone + rim - bowl - basin;
}

MeshData makeTerrain(int worldId) {
  MeshData mesh;
  constexpr int kSegments = 112;
  const float halfExtent = worldId == 12 ? 200.0f : 105.0f;
  const float centerZ = worldId == 12 ? -70.0f : -40.0f;
  const int rowSize = kSegments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(rowSize * rowSize));
  mesh.indices.reserve(static_cast<std::size_t>(kSegments * kSegments * 6));
  const auto heightAt = [worldId](float x, float z) {
    return worldId == 12 ? world12Height(x, z) : world7Height(x, z);
  };
  for (int row = 0; row <= kSegments; ++row) {
    const float z = centerZ - halfExtent + 2.0f * halfExtent *
        static_cast<float>(row) / static_cast<float>(kSegments);
    for (int column = 0; column <= kSegments; ++column) {
      const float x = -halfExtent + 2.0f * halfExtent *
          static_cast<float>(column) / static_cast<float>(kSegments);
      constexpr float step = 0.25f;
      const Vec3 normal = normalize(Vec3{
          heightAt(x - step, z) - heightAt(x + step, z),
          step * 2.0f,
          heightAt(x, z - step) - heightAt(x, z + step),
      });
      mesh.vertices.push_back(Gl33Vertex{{x, heightAt(x, z), z},
                                          {normal.x, normal.y, normal.z}});
    }
  }
  for (int row = 0; row < kSegments; ++row) {
    for (int column = 0; column < kSegments; ++column) {
      const std::uint32_t a = static_cast<std::uint32_t>(row * rowSize + column);
      const std::uint32_t b = a + static_cast<std::uint32_t>(rowSize);
      mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
    }
  }
  return mesh;
}

void upload(Gl33Mesh& destination, MeshData mesh) {
  destination.upload(mesh.vertices, mesh.indices);
}

constexpr char kWorldVertexShaderSource[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;
void main() {
  vec4 worldPosition = uModel * vec4(aPosition, 1.0);
  vWorldPosition = worldPosition.xyz;
  vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
  vUv = aUv;
  gl_Position = uProjection * uView * worldPosition;
}
)glsl";

constexpr char kWorldFragmentShaderSource[] = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
uniform vec4 uBaseColor;
uniform float uEmissive;
uniform vec3 uLightDirection;
uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform int uUseUnderwaterAbsorption;
uniform float uWaterSurfaceY;
uniform float uAbsorptionDensity;
uniform float uDepthAbsorption;
uniform sampler2D uTexture;
uniform int uUseTexture;
uniform float uUvScale;
out vec4 fragmentColor;
void main() {
  float ndl = dot(normalize(vNormal), normalize(-uLightDirection));
  float diffuse = uUseUnderwaterAbsorption != 0
      ? clamp((ndl + 0.35) / 1.35, 0.0, 1.0)
      : max(ndl, 0.0);
  float ambient = uUseUnderwaterAbsorption != 0 ? 0.32 : 0.24;
  float light = mix(ambient + diffuse * (1.0 - ambient), 1.0, uEmissive);
  vec4 texel = uUseTexture != 0 ? texture(uTexture, vUv * uUvScale) : vec4(1.0);
  vec3 color = uBaseColor.rgb * texel.rgb * light;
  float distanceToCamera = length(vWorldPosition - uCameraPosition);
  float fogAmount;
  if (uUseUnderwaterAbsorption != 0) {
    float cameraDepth = max(0.0, uWaterSurfaceY - uCameraPosition.y);
    float fragmentDepth = max(0.0, uWaterSurfaceY - vWorldPosition.y);
    float meanDepth = min(24.0, 0.5 * (cameraDepth + fragmentDepth));
    float density = uAbsorptionDensity + uDepthAbsorption * meanDepth;
    fogAmount = 1.0 - exp(-density * distanceToCamera);
  } else {
    fogAmount = smoothstep(uFogStart, uFogEnd, distanceToCamera);
  }
  fragmentColor = vec4(mix(color, uFogColor, fogAmount), uBaseColor.a);
}
)glsl";

constexpr char kPortalFragmentShaderSource[] = R"glsl(#version 330 core
in vec2 vUv;
uniform vec4 uBaseColor;
uniform float uTime;
uniform float uPortalSeed;
uniform int uGlowPass;
out vec4 fragmentColor;

const float kPi = 3.14159265359;
const float kTwoPi = 6.28318530718;

float band(float value, float center, float halfWidth, float feather) {
  return 1.0 - smoothstep(halfWidth, halfWidth + feather,
                          abs(value - center));
}

float segmentLine(vec2 point, vec2 startPoint, vec2 endPoint, float width) {
  vec2 startToPoint = point - startPoint;
  vec2 segment = endPoint - startPoint;
  float projection = clamp(dot(startToPoint, segment) /
                           max(dot(segment, segment), 0.0001), 0.0, 1.0);
  float distanceToSegment = length(startToPoint - segment * projection);
  return 1.0 - smoothstep(width, width * 2.15, distanceToSegment);
}

mat2 rotation(float angle) {
  float sine = sin(angle);
  float cosine = cos(angle);
  return mat2(cosine, -sine, sine, cosine);
}

void main() {
  vec2 point = vUv * 2.0 - 1.0;
  float radius = length(point);
  float angle = atan(point.y, point.x);
  float normalizedAngle = (angle + kPi) / kTwoPi;

  // A broad, broken stone-metal chassis. Angular cells keep it architectural
  // rather than reading as a stack of thin neon hoops.
  float outerSilhouette = 1.0 - smoothstep(0.925, 0.975, radius);
  float frameCell = fract(normalizedAngle * 18.0 + 0.37);
  float cellEdge = min(frameCell, 1.0 - frameCell);
  float blockBody = smoothstep(0.022, 0.075, cellEdge);
  float innerFrameCell = fract(normalizedAngle * 13.0 + 0.08);
  float innerCellEdge = min(innerFrameCell, 1.0 - innerFrameCell);
  float innerBlockBody = smoothstep(0.025, 0.095, innerCellEdge);
  float outerPlateBand = smoothstep(0.755, 0.785, radius) * outerSilhouette;
  float innerPlateBand = smoothstep(0.645, 0.675, radius) *
      (1.0 - smoothstep(0.745, 0.775, radius));
  float frame = outerPlateBand * (0.10 + 0.90 * blockBody) +
                innerPlateBand * (0.16 + 0.84 * innerBlockBody);
  blockBody = max(blockBody, innerBlockBody * innerPlateBand);

  float frameFacet = 0.5 + 0.5 * sin(radius * 117.0 +
      floor(normalizedAngle * 18.0) * 1.73);
  vec3 darkMetal = mix(vec3(0.032, 0.012, 0.090),
                       vec3(0.155, 0.070, 0.280), frameFacet * 0.72);
  vec3 color = darkMetal * frame;
  float alpha = frame * 0.98;

  // Recessed conduits and deep seams are sparse highlights inside the mass.
  float conduit = (band(radius, 0.700, 0.005, 0.004) +
                   band(radius, 0.770, 0.004, 0.004) +
                   band(radius, 0.875, 0.005, 0.004)) * frame;
  conduit *= 0.55 + 0.45 * blockBody;
  float radialBrace = 1.0 - smoothstep(0.026, 0.070,
      abs(fract(normalizedAngle * 10.0 + 0.5) - 0.5));
  radialBrace *= smoothstep(0.66, 0.70, radius) *
                 (1.0 - smoothstep(0.89, 0.93, radius));

  vec3 cyanEnergy = mix(vec3(0.05, 0.72, 1.0), uBaseColor.rgb, 0.12);
  vec3 violetEnergy = mix(vec3(0.58, 0.12, 1.0), uBaseColor.rgb, 0.08);
  vec3 energyColor = mix(violetEnergy, cyanEnergy,
      0.58 + 0.30 * sin(angle * 3.0 + uPortalSeed));
  color += energyColor * (conduit * 1.45 + radialBrace * 0.48);
  alpha = max(alpha, max(conduit * 0.96, radialBrace * 0.82));

  // Small inscribed medallions replace the previous row of identical lamps.
  float lockBody = 0.0;
  float lockCore = 0.0;
  float lockGlyph = 0.0;
  float lockHalo = 0.0;
  for (int index = 0; index < 8; ++index) {
    float lockAngle = kTwoPi * float(index) / 8.0;
    vec2 radialDirection = vec2(cos(lockAngle), sin(lockAngle));
    vec2 tangentDirection = vec2(-radialDirection.y, radialDirection.x);
    vec2 lockCenter = radialDirection * 0.815;
    vec2 lockPoint = point - lockCenter;
    float lockRadius = length(lockPoint);
    lockBody = max(lockBody, band(lockRadius, 0.052, 0.008, 0.006));
    lockCore = max(lockCore, 1.0 - smoothstep(0.007, 0.018, lockRadius));
    lockHalo = max(lockHalo, 1.0 - smoothstep(0.025, 0.125, lockRadius));
    float radialStroke = 1.0 - smoothstep(0.004, 0.011,
        abs(dot(lockPoint, tangentDirection)));
    radialStroke *= 1.0 - smoothstep(0.018, 0.040,
        abs(dot(lockPoint, radialDirection)));
    float tangentStroke = 1.0 - smoothstep(0.004, 0.011,
        abs(dot(lockPoint, radialDirection)));
    tangentStroke *= 1.0 - smoothstep(0.012, 0.030,
        abs(dot(lockPoint, tangentDirection)));
    lockGlyph = max(lockGlyph,
        index % 2 == 0 ? radialStroke : max(radialStroke, tangentStroke));
  }
  color = mix(color, vec3(0.035, 0.025, 0.090), lockBody * 0.88);
  color += energyColor * (lockBody * 0.75 + lockGlyph * 1.3 + lockCore * 1.8);
  alpha = max(alpha, lockBody * 0.99);
  alpha = max(alpha, lockGlyph * 0.86);

  float verticalAxis = segmentLine(point, vec2(0.0, -0.915),
                                   vec2(0.0, 0.915), 0.0035);
  float horizontalAxis = segmentLine(point, vec2(-0.915, 0.0),
                                     vec2(0.915, 0.0), 0.0025);
  float axisMask = verticalAxis + horizontalAxis * 0.66;
  color += mix(violetEnergy, vec3(0.85, 0.92, 1.0), 0.48) *
           axisMask * 1.15;
  alpha = max(alpha, clamp(axisMask, 0.0, 0.86));

  // A translucent event-horizon membrane: dark depth, restrained turbulence,
  // and localized lightning instead of a flat glowing disk.
  float membrane = 1.0 - smoothstep(0.635, 0.675, radius);
  vec2 flowingPoint = rotation(-uTime * 0.055 - uPortalSeed) * point;
  float flowingAngle = atan(flowingPoint.y, flowingPoint.x);
  float turbulence = 0.5 + 0.5 * sin(radius * 37.0 - uTime * 1.25 +
      sin(flowingAngle * 7.0 + uTime * 0.37) * 2.4);
  float lightning = 1.0 - smoothstep(0.025, 0.095,
      abs(sin(flowingAngle * 5.0 + radius * 23.0 - uTime * 1.6)));
  lightning *= smoothstep(0.20, 0.58, radius) *
               (1.0 - smoothstep(0.61, 0.68, radius));
  float violetCloud = 0.5 + 0.5 * sin(point.x * 11.0 - point.y * 7.0 +
      uTime * 0.44 + sin(flowingAngle * 4.0) * 2.0);
  vec3 membraneColor = mix(vec3(0.020, 0.004, 0.075),
                           mix(violetEnergy, cyanEnergy, violetCloud) * 0.58,
                           turbulence * 0.78);
  color += membraneColor * membrane * (0.82 + turbulence * 0.42);
  color += energyColor * lightning * membrane * 1.45;
  alpha = max(alpha, membrane * (0.43 + turbulence * 0.18));
  alpha = max(alpha, lightning * membrane * 0.72);

  // A slowly counter-rotating geometric stabilizer gives the gate a distinct
  // machine-like identity and echoes the supplied ceremonial reference.
  vec2 latticePoint = rotation(uTime * 0.075 + uPortalSeed * 0.41) * point;
  float lattice = 0.0;
  float latticeHalo = 0.0;
  float nodeGlow = 0.0;
  float nodeRing = 0.0;
  for (int index = 0; index < 6; ++index) {
    float currentAngle = kTwoPi * float(index) / 6.0 + 0.5 * kPi;
    float nextAngle = kTwoPi * float(index + 1) / 6.0 + 0.5 * kPi;
    vec2 currentNode = vec2(cos(currentAngle), sin(currentAngle)) * 0.405;
    vec2 nextNode = vec2(cos(nextAngle), sin(nextAngle)) * 0.405;
    vec2 currentInnerNode = vec2(cos(currentAngle), sin(currentAngle)) * 0.205;
    vec2 nextInnerNode = vec2(cos(nextAngle), sin(nextAngle)) * 0.205;
    lattice = max(lattice,
                  segmentLine(latticePoint, currentNode, nextNode, 0.006));
    lattice = max(lattice,
                  segmentLine(latticePoint, currentNode, -currentNode, 0.004));
    lattice = max(lattice,
                  segmentLine(latticePoint, currentInnerNode,
                              nextInnerNode, 0.004));
    lattice = max(lattice,
                  segmentLine(latticePoint, currentNode,
                              nextInnerNode, 0.0035));
    latticeHalo = max(latticeHalo,
                      segmentLine(latticePoint, currentNode,
                                  nextNode, 0.021));
    latticeHalo = max(latticeHalo,
                      segmentLine(latticePoint, currentNode,
                                  nextInnerNode, 0.016));
    float nodeDistance = length(latticePoint - currentNode);
    float innerNodeDistance = length(latticePoint - currentInnerNode);
    nodeRing = max(nodeRing, band(nodeDistance, 0.052, 0.004, 0.005));
    nodeRing = max(nodeRing,
                   band(innerNodeDistance, 0.031, 0.003, 0.004));
    nodeGlow = max(nodeGlow,
                   (1.0 - smoothstep(0.018, 0.050, nodeDistance)) * 1.25);
  }
  float axialLine = segmentLine(latticePoint, vec2(0.0, -0.585),
                                vec2(0.0, 0.585), 0.0035);
  lattice = max(lattice, axialLine);
  float centralCore = 1.0 - smoothstep(0.018, 0.100, radius);
  float coreHalo = 1.0 - smoothstep(0.02, 0.23, radius);
  float latticeAngle = atan(latticePoint.y, latticePoint.x);
  float roseRadius = 0.265 + 0.072 * cos(latticeAngle * 6.0 - uTime * 0.11);
  float roseLattice = band(length(latticePoint), roseRadius, 0.0035, 0.005);
  lattice = max(lattice, roseLattice);
  color += mix(violetEnergy, cyanEnergy, 0.62) * lattice * 1.55;
  color += mix(cyanEnergy, vec3(1.0), 0.46) * nodeRing * 1.30;
  color += mix(energyColor, vec3(1.0), 0.72) * nodeGlow * 1.55;
  color += mix(energyColor, vec3(1.0), 0.86) * centralCore * 3.2;
  color += energyColor * coreHalo * 0.68;
  alpha = max(alpha, max(lattice * 0.88, nodeGlow));
  alpha = max(alpha, nodeRing * 0.88);
  alpha = max(alpha, max(centralCore, coreHalo * 0.55));

  // Short abstract glyph strokes orbit the aperture; there is no fake text.
  float glyphCell = fract(normalizedAngle * 24.0 + 0.5);
  float glyphStroke = 1.0 - smoothstep(0.10, 0.22,
      abs(glyphCell - 0.5));
  float glyphBand = band(radius, 0.605 +
      0.012 * sin(floor(normalizedAngle * 24.0) * 2.17), 0.005, 0.006);
  float glyphs = glyphStroke * glyphBand;
  float counterGlyphCell = fract(normalizedAngle * 17.0 - uTime * 0.012 + 0.2);
  float counterGlyphs = (1.0 - smoothstep(0.08, 0.17,
      abs(counterGlyphCell - 0.5))) * band(radius, 0.735, 0.003, 0.005);
  color += mix(violetEnergy, cyanEnergy, 0.64) * glyphs * 1.35;
  color += violetEnergy * counterGlyphs * 1.05;
  alpha = max(alpha, glyphs * 0.82);
  alpha = max(alpha, counterGlyphs * 0.72);

  float apertureRim = band(radius, 0.655, 0.008, 0.016);
  float outerGlow = (1.0 - smoothstep(0.76, 1.02, radius)) *
                    smoothstep(0.57, 0.70, radius);
  color += energyColor * (apertureRim * 1.9 + outerGlow * 0.19);
  alpha = max(alpha, apertureRim * 0.94);
  alpha = max(alpha, outerGlow * 0.12);

  float apertureHalo = exp(-abs(radius - 0.655) * 18.0);
  float frameHalo = exp(-abs(radius - 0.790) * 8.0) * 0.22;
  float glowMask = apertureHalo * 0.78 + frameHalo + lockHalo * 0.30 +
      latticeHalo * 0.18 + nodeGlow * 0.34 + coreHalo * 0.92 +
      lightning * 0.31 + conduit * 0.24 + glyphs * 0.22;
  float pulse = 0.93 + 0.07 * sin(uTime * 2.0 + uPortalSeed * 5.0);
  if (uGlowPass != 0) {
    glowMask *= pulse;
    if (glowMask < 0.012 || radius > 0.995) {
      discard;
    }
    vec3 glowColor = mix(violetEnergy, cyanEnergy,
        0.5 + 0.35 * sin(angle * 2.0 - uTime * 0.18));
    fragmentColor = vec4(glowColor * glowMask * 4.0,
                         clamp(glowMask * 0.52, 0.0, 0.82));
    return;
  }
  color *= pulse;
  alpha *= outerSilhouette;
  if (alpha < 0.008) {
    discard;
  }
  fragmentColor = vec4(color, clamp(alpha * uBaseColor.a, 0.0, 1.0));
}
)glsl";

struct WorldResources {
  Gl33Mesh cube;
  Gl33Mesh pyramid;
  Gl33Mesh cylinder;
  Gl33Mesh crystal;
  Gl33Mesh annulus;
  Gl33Mesh portalQuad;
  Gl33Mesh sphere;
  Gl33Mesh reefRock;
  Gl33Mesh terrain;
  bool hasTerrain = false;
  Gl33ShaderProgram program;
  Gl33ShaderProgram portalProgram;
  Gl33Texture wallTexture;
  Gl33Texture panelTexture;
  bool hasCorridorTextures = false;
};

class Painter {
public:
  Painter(WorldResources& resources, const Gl33WorldFrame& frame,
          Color fogColor, float fogStart, float fogEnd,
          bool useUnderwaterAbsorption = false)
      : resources_(resources) {
    const Vec3 eye{frame.camera.position[0], frame.camera.position[1], frame.camera.position[2]};
    const Vec3 front{frame.camera.front[0], frame.camera.front[1], frame.camera.front[2]};
    const Vec3 up{frame.camera.up[0], frame.camera.up[1], frame.camera.up[2]};
    const float aspect = static_cast<float>(std::max(frame.camera.framebufferWidth, 1)) /
        static_cast<float>(std::max(frame.camera.framebufferHeight, 1));
    view_ = view(eye, front, up);
    projection_ = perspective(60.0f * kPi / 180.0f, aspect, 0.1f, 520.0f);
    resources_.program.use();
    resources_.program.setMatrix4("uView", view_.data());
    resources_.program.setMatrix4("uProjection", projection_.data());
    resources_.program.setVec3("uCameraPosition", eye.x, eye.y, eye.z);
    resources_.program.setVec3("uLightDirection", -0.32f, -0.88f, -0.36f);
    resources_.program.setVec3("uFogColor", fogColor.red, fogColor.green, fogColor.blue);
    resources_.program.setFloat("uFogStart", fogStart);
    resources_.program.setFloat("uFogEnd", fogEnd);
    resources_.program.setInt("uUseUnderwaterAbsorption",
                              useUnderwaterAbsorption ? 1 : 0);
    resources_.program.setFloat("uWaterSurfaceY", 12.5f);
    resources_.program.setFloat("uAbsorptionDensity", 0.008f);
    resources_.program.setFloat("uDepthAbsorption", 0.0017f);
    if (useUnderwaterAbsorption) setWorld11Environment(resources_.program);
  }

  void mesh(Gl33Mesh& meshValue, const Matrix4& model, Color color,
            float emissive = 0.0f) {
    resources_.program.setMatrix4("uModel", model.data());
    resources_.program.setVec4("uBaseColor", color.red, color.green, color.blue, color.alpha);
    resources_.program.setFloat("uEmissive", emissive);
    resources_.program.setInt("uUseTexture", 0);
    resources_.program.setFloat("uUvScale", 1.0f);
    meshValue.draw();
  }

  void texturedMesh(Gl33Mesh& meshValue, const Matrix4& model,
                    Gl33Texture& texture, Color color, float uvScale = 1.0f,
                    float emissive = 0.0f) {
    resources_.program.setMatrix4("uModel", model.data());
    resources_.program.setVec4("uBaseColor", color.red, color.green, color.blue, color.alpha);
    resources_.program.setFloat("uEmissive", emissive);
    resources_.program.setInt("uUseTexture", 1);
    resources_.program.setInt("uTexture", 0);
    resources_.program.setFloat("uUvScale", uvScale);
    texture.bind(0);
    meshValue.draw();
  }

  void box(float x, float bottomY, float z, float sizeX, float sizeY, float sizeZ,
           Color color, float emissive = 0.0f, float yaw = 0.0f) {
    mesh(resources_.cube,
         modelMatrix(x, bottomY + sizeY * 0.5f, z, sizeX, sizeY, sizeZ, 0.0f, yaw),
         color, emissive);
  }

  void pyramid(float x, float bottomY, float z, float size, Color color) {
    mesh(resources_.pyramid, modelMatrix(x, bottomY, z, size, size, size), color);
  }

  void texturedBox(float x, float bottomY, float z,
                   float sizeX, float sizeY, float sizeZ,
                   Gl33Texture& texture, Color color, float uvScale = 1.0f,
                   float emissive = 0.0f) {
    texturedMesh(resources_.cube,
                 modelMatrix(x, bottomY + sizeY * 0.5f, z,
                             sizeX, sizeY, sizeZ),
                 texture, color, uvScale, emissive);
  }

  void cylinder(float x, float bottomY, float z, float radius, float height,
                Color color, float emissive = 0.0f) {
    mesh(resources_.cylinder,
         modelMatrix(x, bottomY, z, radius, height, radius), color, emissive);
  }

  void crystal(float x, float bottomY, float z, float radius, float height,
               Color color, float emissive = 0.0f, float yaw = 0.0f) {
    mesh(resources_.crystal,
         modelMatrix(x, bottomY, z, radius, height, radius, 0.0f, yaw), color, emissive);
  }

  void sphere(float x, float y, float z, float radius, Color color,
              float emissive = 0.0f) {
    mesh(resources_.sphere,
         modelMatrix(x, y, z, radius, radius, radius), color, emissive);
  }

  void reefRock(float x, float y, float z, float width, float height,
                float depth, Color color, float emissive = 0.0f,
                float rotateX = 0.0f, float rotateY = 0.0f,
                float rotateZ = 0.0f) {
    mesh(resources_.reefRock,
         modelMatrix(x, y, z, width, height, depth,
                     rotateX, rotateY, rotateZ),
         color, emissive);
  }

  void ring(float x, float y, float z, float radius, Color color,
            bool horizontal = false, float emissive = 1.0f) {
    mesh(resources_.annulus,
         modelMatrix(x, y, z, radius, radius, radius,
                     horizontal ? 0.5f * kPi : 0.0f),
         color, emissive);
  }

  void portal(float x, float y, float z, float radius, Color color,
              float time, float seed) {
    Gl33Api& gl = api();
    gl.Enable(kBlend);
    gl.BlendFunc(kSourceAlpha, kOneMinusSourceAlpha);
    // The World 11 water compositor samples the opaque-pass depth texture.
    // Keep the gate in that depth buffer so water geometry that is farther
    // away cannot be composited over the portal's frame or event horizon.
    gl.DepthMask(kTrue);
    gl.DepthFunc(kLessEqual);
    gl.Disable(kCullFace);

    resources_.portalProgram.use();
    resources_.portalProgram.setMatrix4("uView", view_.data());
    resources_.portalProgram.setMatrix4("uProjection", projection_.data());
    resources_.portalProgram.setMatrix4(
        "uModel", modelMatrix(x, y, z, radius, radius, 1.0f).data());
    resources_.portalProgram.setVec4(
        "uBaseColor", color.red, color.green, color.blue, color.alpha);
    resources_.portalProgram.setFloat("uTime", time);
    resources_.portalProgram.setFloat("uPortalSeed", seed);
    resources_.portalProgram.setInt("uGlowPass", 0);
    resources_.portalQuad.draw();

    // A second additive pass supplies the cyan-violet bloom that the
    // ceremonial reference gets from many overlapping luminous layers.
    gl.DepthMask(kFalse);
    gl.BlendFunc(kSourceAlpha, kOne);
    resources_.portalProgram.setInt("uGlowPass", 1);
    resources_.portalQuad.draw();

    gl.DepthMask(kTrue);
    gl.Disable(kBlend);
    resources_.program.use();
  }

  void terrain(Color color) {
    if (resources_.hasTerrain) {
      mesh(resources_.terrain, identityMatrix(), color);
    }
  }

  WorldResources& resources() {
    return resources_;
  }

private:
  WorldResources& resources_;
  Matrix4 view_{};
  Matrix4 projection_{};
};

Color portalColor(int worldId) {
  constexpr std::array<Color, 7> colors = {
      Color{0.20f, 0.85f, 1.00f, 0.94f},
      Color{0.25f, 0.95f, 0.88f, 0.94f},
      Color{0.78f, 0.90f, 1.00f, 0.94f},
      Color{0.96f, 0.62f, 0.18f, 0.94f},
      Color{0.30f, 0.95f, 0.90f, 0.94f},
      Color{0.05f, 0.68f, 0.82f, 0.94f},
      Color{1.00f, 0.28f, 0.06f, 0.94f},
  };
  const int index = std::clamp(worldId - 6, 0, static_cast<int>(colors.size()) - 1);
  return colors[static_cast<std::size_t>(index)];
}

void drawPortals(Painter& painter, const Gl33WorldFrame& frame) {
  for (std::size_t index = 0; index < frame.portalCount; ++index) {
    const world::Portal& portal = frame.portals[index];
    const Color color = portalColor(portal.target_world_id);
    const float seed = static_cast<float>(index) * 0.73f +
        static_cast<float>(portal.target_world_id) * 0.19f;
    painter.portal(portal.x, portal.y, portal.z, portal.radius, color,
                   frame.time, seed);
  }
}

void drawWorld6(Painter& painter, const Gl33WorldFrame& frame) {
  painter.pyramid(-4.0f, -1.0f, -100.0f, 2.0f, {0.90f, 0.82f, 0.48f});
  painter.pyramid(4.0f, -1.0f, -10.0f, 2.0f, {0.85f, 0.74f, 0.36f});
  painter.pyramid(0.0f, -10.0f, -10.0f, 2.0f, {0.72f, 0.62f, 0.30f});
  painter.pyramid(0.0f, -10.0f, -20.0f, 2.0f, {0.72f, 0.62f, 0.30f});
  drawPortals(painter, frame);
}

void drawWorld7(Painter& painter, const Gl33WorldFrame& frame) {
  painter.terrain({0.055f, 0.065f, 0.085f});
  constexpr std::array<Vec3, 4> monoliths = {
      Vec3{-4.5f, -1.0f, -12.0f}, Vec3{4.5f, -1.0f, -12.0f},
      Vec3{-4.5f, -1.0f, -6.0f}, Vec3{4.5f, -1.0f, -6.0f},
  };
  for (const Vec3& item : monoliths) {
    painter.box(item.x, item.y, item.z, 0.7f, 3.8f, 0.7f,
                {0.08f, 0.30f, 0.36f}, 0.25f);
  }
  painter.box(0.0f, -1.0f, -9.0f, 12.0f, 0.6f, 12.0f, {0.08f, 0.11f, 0.15f});
  painter.box(0.0f, -0.4f, -9.0f, 8.2f, 0.55f, 8.2f, {0.10f, 0.15f, 0.19f});
  painter.box(0.0f, 0.15f, -9.0f, 4.2f, 0.45f, 4.2f, {0.12f, 0.22f, 0.25f});
  painter.box(17.0f, -1.0f, -11.5f, 10.0f, 0.55f, 8.0f, {0.16f, 0.095f, 0.05f});
  painter.box(17.0f, -0.45f, -11.5f, 6.6f, 0.45f, 5.6f, {0.24f, 0.12f, 0.045f});
  painter.box(-12.0f, -1.0f, -42.0f, 6.0f, 18.0f, 8.0f, {0.055f, 0.07f, 0.09f});
  painter.box(12.0f, -1.0f, -42.0f, 6.0f, 18.0f, 8.0f, {0.055f, 0.07f, 0.09f});
  painter.box(0.0f, 17.0f, -42.0f, 30.0f, 6.0f, 8.0f, {0.07f, 0.08f, 0.10f});
  painter.box(0.0f, 23.0f, -42.0f, 14.0f, 5.0f, 10.0f, {0.09f, 0.11f, 0.13f});
  for (int beacon = 0; beacon < 8; ++beacon) {
    const float angle = kTwoPi * static_cast<float>(beacon) / 8.0f;
    painter.cylinder(std::cos(angle) * 10.0f, -1.0f,
                     -9.0f + std::sin(angle) * 10.0f,
                     0.24f, 2.8f, {0.12f, 0.85f, 0.94f}, 0.85f);
  }
  for (int particle = 0; particle < 24; ++particle) {
    const float phase = frame.time * (0.8f + 0.03f * particle) + particle * 1.7f;
    const float centerX = (particle % 2 == 0) ? 0.0f : 17.0f;
    const float centerZ = (particle % 2 == 0) ? -9.0f : -11.5f;
    painter.sphere(centerX + std::sin(phase * 1.3f) * 2.2f,
                   1.2f + std::fmod(frame.time * 1.4f + particle * 0.37f, 5.0f),
                   centerZ + std::cos(phase) * 2.0f,
                   0.07f + 0.025f * static_cast<float>(particle % 3),
                   {1.0f, 0.38f, 0.06f}, 1.0f);
  }
  drawPortals(painter, frame);
}

void drawWorld8(Painter& painter, const Gl33WorldFrame& frame) {
  painter.box(0.0f, -1.25f, 0.0f, 160.0f, 0.25f, 160.0f,
              {0.05f, 0.07f, 0.09f});
  for (int line = -10; line <= 10; ++line) {
    const float coordinate = static_cast<float>(line) * 4.0f;
    painter.box(coordinate, -0.995f, 0.0f, 0.025f, 0.012f, 160.0f,
                {0.14f, 0.24f, 0.32f}, 0.5f);
    painter.box(0.0f, -0.995f, coordinate, 160.0f, 0.012f, 0.025f,
                {0.14f, 0.24f, 0.32f}, 0.5f);
  }
  painter.box(0.0f, -0.98f, -14.0f, 16.0f, 0.018f, 0.04f,
              {0.72f, 0.88f, 1.0f}, 0.8f);
  drawPortals(painter, frame);
}

void drawWorld9(Painter& painter, const Gl33WorldFrame& frame) {
  painter.box(0.0f, -1.20f, -20.0f, 5.1f, 0.20f, 60.0f, {0.10f, 0.12f, 0.14f});
  painter.box(0.0f, 2.45f, -20.0f, 5.1f, 0.16f, 60.0f, {0.08f, 0.10f, 0.12f});
  if (painter.resources().hasCorridorTextures) {
    painter.texturedBox(-2.65f, -1.0f, -20.0f, 0.20f, 3.45f, 60.0f,
                        painter.resources().wallTexture, {0.82f, 0.88f, 0.92f}, 6.0f);
    painter.texturedBox(2.65f, -1.0f, -20.0f, 0.20f, 3.45f, 60.0f,
                        painter.resources().wallTexture, {0.82f, 0.88f, 0.92f}, 6.0f);
  } else {
    painter.box(-2.65f, -1.0f, -20.0f, 0.20f, 3.45f, 60.0f, {0.09f, 0.12f, 0.14f});
    painter.box(2.65f, -1.0f, -20.0f, 0.20f, 3.45f, 60.0f, {0.09f, 0.12f, 0.14f});
  }
  for (int rib = 0; rib < 11; ++rib) {
    const float z = 7.0f - static_cast<float>(rib) * 5.0f;
    painter.box(-2.35f, -0.92f, z, 0.10f, 3.22f, 0.18f,
                {0.20f, 0.42f, 0.48f}, 0.35f);
    painter.box(2.35f, -0.92f, z, 0.10f, 3.22f, 0.18f,
                {0.20f, 0.42f, 0.48f}, 0.35f);
    if (rib % 2 == 0) {
      painter.box(0.0f, 2.29f, z, 1.8f, 0.08f, 0.75f,
                  {0.25f, 1.0f, 1.0f}, 1.0f);
    }
  }
  const float doorGap = frame.doorOpenAmount * 2.2f;
  painter.box(-1.30f - doorGap, -1.0f, -43.95f, 2.55f, 3.45f, 0.22f,
              {0.20f, 0.24f, 0.27f});
  painter.box(1.30f + doorGap, -1.0f, -43.95f, 2.55f, 3.45f, 0.22f,
              {0.20f, 0.24f, 0.27f});
  for (int terminal = 0; terminal < 5; ++terminal) {
    const float z = -4.0f - terminal * 8.0f;
    const bool left = terminal % 2 == 0;
    if (painter.resources().hasCorridorTextures) {
      painter.texturedBox(left ? -2.52f : 2.52f, -0.1f, z,
                          0.06f, 1.1f, 1.5f,
                          painter.resources().panelTexture,
                          {0.72f, 0.94f, 1.0f}, 1.0f, 0.35f);
    } else {
      painter.box(left ? -2.52f : 2.52f, -0.1f, z,
                  0.06f, 1.1f, 1.5f, {0.18f, 0.72f, 0.92f}, 0.9f);
    }
  }
  drawPortals(painter, frame);
}

Color crystalColor(float tint) {
  return Color{0.14f + tint * 0.20f, 0.48f + tint * 0.22f,
               0.82f - tint * 0.12f, 1.0f};
}

void drawWorld10(Painter& painter, const Gl33WorldFrame& frame) {
  painter.box(0.0f, -1.25f, -24.0f, 120.0f, 0.25f, 120.0f,
              {0.04f, 0.06f, 0.10f});
  constexpr std::array<std::array<float, 5>, 14> crystals = {{
      {-6.5f, -12.0f, 0.55f, 2.2f, 0.00f}, {6.0f, -16.5f, 0.75f, 3.1f, 0.30f},
      {-10.0f, -20.0f, 0.90f, 3.8f, 0.60f}, {9.5f, -23.0f, 0.60f, 2.4f, 0.20f},
      {-14.5f, -14.0f, 0.70f, 2.9f, 0.50f}, {14.0f, -12.0f, 0.80f, 3.3f, 0.40f},
      {-8.5f, -30.0f, 0.65f, 2.6f, 0.10f}, {8.0f, -34.0f, 0.85f, 3.6f, 0.55f},
      {-13.0f, -38.0f, 1.00f, 4.2f, 0.35f}, {13.5f, -40.0f, 0.70f, 2.8f, 0.65f},
      {-4.5f, -44.0f, 0.80f, 3.4f, 0.25f}, {4.0f, -48.0f, 0.95f, 4.0f, 0.50f},
      {-11.0f, -52.0f, 0.60f, 2.3f, 0.45f}, {11.5f, -55.0f, 0.75f, 3.0f, 0.15f},
  }};
  for (std::size_t index = 0; index < crystals.size(); ++index) {
    const auto& item = crystals[index];
    painter.crystal(item[0], -1.0f, item[1], item[2], item[3],
                    crystalColor(item[4]), 0.38f, static_cast<float>(index) * 0.37f);
  }
  painter.crystal(0.0f, -1.0f, -32.0f, 1.7f, 9.0f,
                  {0.28f, 0.72f, 0.98f}, 0.55f);
  for (int shard = 0; shard < 8; ++shard) {
    const float angle = frame.time * 0.9f + kTwoPi * static_cast<float>(shard) / 8.0f;
    painter.crystal(std::cos(angle) * 3.4f, 4.9f + std::sin(angle * 2.0f) * 0.35f,
                    -32.0f + std::sin(angle) * 3.4f,
                    0.22f, 1.0f, {0.38f, 0.92f, 1.0f}, 0.8f, -angle);
  }
  for (int ring = 0; ring < 3; ++ring) {
    painter.ring(0.0f, -0.90f + ring * 0.018f, -32.0f,
                 4.5f + ring * 2.2f, {0.18f, 0.72f, 0.94f, 0.34f}, true);
  }
  drawPortals(painter, frame);
}

void drawWorld12(Painter& painter, const Gl33WorldFrame& frame) {
  painter.terrain({0.13f, 0.105f, 0.095f});
  painter.cylinder(0.0f, 26.36f, -112.0f, 11.5f, 0.10f,
                   {1.0f, 0.22f, 0.015f}, 1.0f);
  painter.cylinder(24.0f, -2.34f, -36.0f, 11.5f, 0.10f,
                   {1.0f, 0.30f, 0.02f}, 1.0f);
  constexpr std::array<Vec3, 12> columns = {
      Vec3{-18.0f, 5.2f, -24.0f}, Vec3{-21.0f, 6.4f, -29.0f},
      Vec3{38.0f, 7.0f, -54.0f}, Vec3{35.0f, 8.2f, -58.0f},
      Vec3{-42.0f, 8.8f, -67.0f}, Vec3{-40.0f, 7.1f, -75.0f},
      Vec3{27.0f, 6.8f, -86.0f}, Vec3{31.0f, 9.3f, -89.0f},
      Vec3{-31.0f, 9.0f, -105.0f}, Vec3{25.0f, 8.0f, -121.0f},
      Vec3{22.0f, 6.4f, -126.0f}, Vec3{-15.0f, 7.5f, -139.0f},
  };
  for (std::size_t index = 0; index < columns.size(); ++index) {
    const Vec3& column = columns[index];
    painter.cylinder(column.x, world12Height(column.x, column.z), column.z,
                     0.9f + 0.12f * static_cast<float>(index % 4), column.y,
                     {0.11f, 0.095f, 0.09f});
  }
  for (int ember = 0; ember < 70; ++ember) {
    const bool crater = ember % 3 != 0;
    const float centerX = crater ? 0.0f : 24.0f;
    const float centerY = crater ? 26.4f : -2.3f;
    const float centerZ = crater ? -112.0f : -36.0f;
    const float phase = ember * 2.17f + frame.time * (0.8f + (ember % 5) * 0.08f);
    const float radius = crater ? 7.0f : 8.0f;
    painter.sphere(centerX + std::sin(phase) * radius * 0.55f,
                   centerY + std::fmod(frame.time * 1.4f + ember * 0.29f, 5.0f),
                   centerZ + std::cos(phase * 1.13f) * radius * 0.55f,
                   0.045f + 0.018f * (ember % 3),
                   {1.0f, 0.30f, 0.025f}, 1.0f);
  }
  drawPortals(painter, frame);
}

void drawWorld11Landmarks(Painter& painter, const Gl33WorldFrame& frame) {
  const auto arch = world::kWorld11Arch;
  const float archFloor = world::world11SeabedHeight(arch.x, arch.z);

  auto drawArchStone = [&](const std::array<float, 10>& piece) {
    const float x = arch.x + piece[0];
    const float y = archFloor + piece[1];
    const float z = arch.z + piece[2];
    const float tone = piece[9];

    painter.reefRock(
        x, y, z,
        piece[3], piece[4], piece[5],
        {0.24f * tone, 0.31f * tone, 0.29f * tone, 1.0f},
        0.0f,
        piece[6], piece[7], piece[8]);
  };

  // Authored rubble arch: narrow piers and a segmented crown produce a clear
  // architectural opening instead of a continuous semi-elliptic "sausage".
  constexpr std::array<std::array<float, 10>, 4> leftPier = {{
      {-5.10f, 0.24f, -0.10f, 2.25f, 0.62f, 2.00f,  0.03f, -0.10f, -0.05f, 0.92f},
      {-4.30f, 1.00f, -0.08f, 1.72f, 1.00f, 1.58f, -0.04f,  0.08f, -0.14f, 0.97f},
      {-3.70f, 2.10f,  0.04f, 1.52f, 1.08f, 1.40f,  0.08f,  0.16f, -0.26f, 1.02f},
      {-3.05f, 3.12f,  0.10f, 1.36f, 0.94f, 1.24f,  0.12f,  0.18f, -0.48f, 1.00f},
  }};

  constexpr std::array<std::array<float, 10>, 4> rightPier = {{
      { 5.00f, 0.22f,  0.14f, 2.10f, 0.58f, 1.90f, -0.02f,  0.12f,  0.05f, 0.90f},
      { 4.22f, 0.98f,  0.06f, 1.66f, 0.98f, 1.52f,  0.05f, -0.06f,  0.13f, 0.95f},
      { 3.62f, 2.04f, -0.02f, 1.46f, 1.08f, 1.34f, -0.09f, -0.16f,  0.28f, 1.00f},
      { 3.04f, 3.02f, -0.08f, 1.30f, 0.92f, 1.18f, -0.12f, -0.20f,  0.50f, 0.98f},
  }};

  constexpr std::array<std::array<float, 10>, 6> archCrown = {{
      {-2.22f, 3.92f,  0.04f, 1.62f, 0.86f, 1.22f,  0.10f,  0.04f, 0.86f, 1.02f},
      {-1.18f, 4.74f,  0.08f, 1.72f, 0.82f, 1.22f, -0.04f,  0.00f, 1.12f, 1.06f},
      {-0.10f, 5.20f,  0.10f, 1.78f, 0.80f, 1.28f,  0.02f,  0.00f, 1.46f, 1.10f},
      { 0.96f, 5.06f,  0.04f, 1.74f, 0.80f, 1.24f,  0.02f,  0.02f, 1.72f, 1.08f},
      { 1.92f, 4.34f, -0.02f, 1.62f, 0.84f, 1.18f, -0.05f,  0.02f, 2.02f, 1.02f},
      { 2.58f, 3.58f, -0.06f, 1.34f, 0.84f, 1.10f, -0.08f,  0.04f, 2.28f, 0.98f},
  }};

  constexpr std::array<std::array<float, 10>, 1> keystone = {{
      {0.06f, 5.78f, 0.02f, 0.98f, 0.44f, 0.86f, 0.00f, 0.02f, 1.56f, 1.08f},
  }};

  for (const auto& piece : leftPier) drawArchStone(piece);
  for (const auto& piece : rightPier) drawArchStone(piece);
  for (const auto& piece : archCrown) drawArchStone(piece);
  for (const auto& piece : keystone) drawArchStone(piece);

  // Low rubble broadens the contact with the seabed without creating two
  // oversized "feet".
  constexpr std::array<std::array<float, 10>, 5> rubble = {{
      {-6.15f, 0.14f,  1.00f, 1.00f, 0.34f, 0.86f,  0.02f, -0.10f, -0.12f, 0.90f},
      {-5.72f, 0.10f, -1.08f, 0.82f, 0.28f, 0.74f, -0.04f,  0.12f,  0.08f, 0.88f},
      { 5.86f, 0.12f, -0.98f, 0.92f, 0.30f, 0.78f,  0.03f, -0.08f,  0.10f, 0.89f},
      { 6.22f, 0.10f,  0.82f, 0.78f, 0.26f, 0.66f, -0.02f,  0.10f, -0.08f, 0.87f},
      { 0.10f, 0.06f,  2.35f, 0.90f, 0.18f, 0.64f,  0.01f,  0.04f,  0.00f, 0.86f},
  }};

  for (const auto& piece : rubble) {
    const float x = arch.x + piece[0];
    const float z = arch.z + piece[2];
    const float y = world::world11SeabedHeight(x, z) + piece[1];
    painter.reefRock(
        x, y, z,
        piece[3], piece[4], piece[5],
        {0.24f * piece[9], 0.31f * piece[9], 0.29f * piece[9], 1.0f},
        0.0f,
        piece[6], piece[7], piece[8]);
  }

  // Sparse growths sit on the sides of the masonry, not on the front face.
  painter.reefRock(
      arch.x - 2.95f, archFloor + 3.55f, arch.z + 0.72f,
      0.20f, 0.32f, 0.20f,
      {0.78f, 0.45f, 0.30f, 1.0f}, 0.16f,
      0.16f, 0.34f, -0.42f);
  painter.reefRock(
      arch.x - 2.86f, archFloor + 3.84f, arch.z + 0.76f,
      0.11f, 0.20f, 0.11f,
      {0.96f, 0.74f, 0.46f, 1.0f}, 0.34f,
      0.26f, 0.28f, -0.18f);

  painter.reefRock(
      arch.x + 2.44f, archFloor + 4.00f, arch.z - 0.58f,
      0.18f, 0.28f, 0.18f,
      {0.74f, 0.42f, 0.28f, 1.0f}, 0.14f,
      -0.12f, -0.28f, 0.32f);
  painter.reefRock(
      arch.x + 2.34f, archFloor + 4.24f, arch.z - 0.62f,
      0.10f, 0.18f, 0.10f,
      {0.94f, 0.70f, 0.44f, 1.0f}, 0.30f,
      -0.20f, -0.14f, 0.20f);

  const auto seep = world::kWorld11Seep;
  for (int i = 0; i < 5; ++i) {
    const float angle = i * kTwoPi / 5.0f;
    const float x = seep.x + 2.0f * std::cos(angle);
    const float z = seep.z + 2.0f * std::sin(angle);
    const float y = world::world11SeabedHeight(x, z);
    const float height = 1.6f + 0.45f * (i % 3);
    painter.reefRock(x, y + height * 0.45f, z, 0.85f, height, 0.75f,
                     {0.10f, 0.16f, 0.18f});
    painter.ring(x, y + height * 1.4f, z, 0.28f,
                 {0.21f, 0.72f, 0.68f}, true, 0.55f);
  }
  const auto colony = world::kWorld11Colony;
  for (int i = 0; i < 18; ++i) {
    const float angle = i * 2.39996f;
    const float radius = 0.8f * std::sqrt(static_cast<float>(i));
    const float x = colony.x + radius * std::cos(angle);
    const float z = colony.z + radius * std::sin(angle);
    const float y = world::world11SeabedHeight(x, z);
    const float height = 1.5f + 1.1f * (0.5f + 0.5f * std::sin(i * 1.8f));
    painter.reefRock(x, y + height * 0.5f, z, 0.22f, height * 0.6f, 0.22f,
                     {0.15f, 0.32f, 0.43f});
    const float glow = 0.65f + 0.12f * std::sin(frame.time * 0.6f + i * 0.4f);
    painter.reefRock(x, y + height, z, 0.65f, 0.28f, 0.65f,
                     {0.20f, 0.85f, 0.92f}, glow);
  }
  // Sparse warm markers lead back to the portal without forming a HUD overlay.
  for (int i = 0; i < 5; ++i) {
    const float z = -28.0f - i * 7.0f;
    const float x = -3.0f - i * 2.0f;
    painter.sphere(x, world::world11SeabedHeight(x, z) + 0.25f, z,
                   0.30f, {0.92f, 0.59f, 0.20f}, 0.7f);
  }
}

void drawTemplateWorld(Painter& painter, const Gl33WorldFrame& frame) {
  painter.box(0.0f, -1.20f, 0.0f, 128.0f, 0.20f, 128.0f,
              {0.07f, 0.08f, 0.10f});
  for (int line = -8; line <= 8; ++line) {
    const float coordinate = static_cast<float>(line) * 4.0f;
    painter.box(coordinate, -0.995f, 0.0f, 0.025f, 0.012f, 128.0f,
                {0.14f, 0.22f, 0.28f}, 0.35f);
    painter.box(0.0f, -0.995f, coordinate, 128.0f, 0.012f, 0.025f,
                {0.14f, 0.22f, 0.28f}, 0.35f);
  }
  painter.box(0.0f, -0.25f, -10.0f, 3.0f, 0.08f, 0.08f,
              {0.85f, 0.90f, 1.00f}, 0.8f);
  painter.box(0.0f, -1.75f, -10.0f, 0.08f, 3.0f, 0.08f,
              {0.85f, 0.90f, 1.00f}, 0.8f);
  drawPortals(painter, frame);
}

Color clearColorForWorld(int worldId) {
  switch (worldId) {
    case 6: return {0.01f, 0.012f, 0.02f};
    case 7: return {0.012f, 0.018f, 0.032f};
    case 8: return {0.018f, 0.035f, 0.055f};
    case 9: return {0.015f, 0.025f, 0.035f};
    case 10: return {0.02f, 0.05f, 0.09f};
    case 11: return {0.10f, 0.32f, 0.40f};
    case 12: return {0.105f, 0.075f, 0.065f};
    default: return {0.02f, 0.03f, 0.05f};
  }
}

}  // namespace

struct Gl33WorldRenderer::Resources : WorldResources {};

Gl33WorldRenderer::Gl33WorldRenderer(int worldId) : worldId_(worldId) {
  if (worldId_ == 11) {
    oceanRenderer_ = std::make_unique<Gl33Renderer>();
    world11DecorRenderer_ = std::make_unique<Gl33World11DecorRenderer>();
    world11FishRenderer_ = std::make_unique<Gl33World11FishRenderer>();
  }
  switch (worldId_) {
    case 8:
      skybox_ = std::make_unique<Gl33Skybox>(
          "datasets/0x00000012.fget", 240.0f);
      break;
    case 9:
      skybox_ = std::make_unique<Gl33Skybox>(
          "datasets/0x00000013.fget", 240.0f);
      break;
    case 10:
      skybox_ = std::make_unique<Gl33Skybox>(
          "datasets/0x00000016.fget", 240.0f);
      break;
    case 11:
      skybox_ = std::make_unique<Gl33Skybox>(
          "datasets/0x00000017.fget", 200.0f, 0.465f, 0.500f,
          true, 12.5f, std::array<float, 3>{0.10f, 0.32f, 0.40f});
      break;
    default:
      break;
  }
}

Gl33WorldRenderer::~Gl33WorldRenderer() = default;

void Gl33WorldRenderer::setWorld11DecorSeeds(
    world::World11DecorSeeds seeds) {
  if (world11DecorRenderer_ != nullptr) {
    world11DecorRenderer_->setSeeds(seeds);
  }
}

void Gl33WorldRenderer::render(const Gl33WorldFrame& frame) {
  ensureInitialized();
  const Color clearColor = clearColorForWorld(worldId_);
  Gl33Api& gl = api();
  gl.ClearColor(clearColor.red, clearColor.green, clearColor.blue, 1.0f);
  gl.Clear(kColorBufferBit | kDepthBufferBit);
  gl.Enable(kDepthTest);
  gl.Disable(kBlend);
  gl.DepthMask(kTrue);

  const bool depthBasedWater = worldId_ == 11 && oceanRenderer_ != nullptr;
  if (depthBasedWater) {
    oceanRenderer_->beginOpaquePass(frame.camera);
  }

  if (skybox_ != nullptr) {
    const float centerY = worldId_ == 11 ? 12.5f : frame.camera.position[1];
    skybox_->render(frame.camera, centerY);
  }

  if (depthBasedWater) {
    oceanRenderer_->renderSeabed(frame.camera);
    world11DecorRenderer_->render(frame.camera, frame.time);
    world11FishRenderer_->render(frame.camera, frame.time);
  }

  const Color fog = worldId_ == 11 ? Color{0.10f, 0.32f, 0.40f}
      : (worldId_ == 12 ? Color{0.105f, 0.075f, 0.065f}
                        : Color{clearColor.red, clearColor.green, clearColor.blue});
  const float fogEnd = worldId_ == 9 ? 72.0f : (worldId_ == 10 ? 68.0f : 165.0f);
  Painter painter(*resources_, frame, fog, 22.0f, fogEnd, worldId_ == 11);
  switch (worldId_) {
    case 6: drawWorld6(painter, frame); break;
    case 7: drawWorld7(painter, frame); break;
    case 8: drawWorld8(painter, frame); break;
    case 9: drawWorld9(painter, frame); break;
    case 10: drawWorld10(painter, frame); break;
    case 11:
      drawWorld11Landmarks(painter, frame);
      drawPortals(painter, frame);
      break;
    case 12: drawWorld12(painter, frame); break;
    default: drawTemplateWorld(painter, frame); break;
  }
  if (depthBasedWater) {
    oceanRenderer_->renderWaterSurfaceDepth(frame.camera, frame.time);
    const std::uint32_t waterSurfaceDepth =
        oceanRenderer_->waterSurfaceDepthTexture();
    world11DecorRenderer_->renderBubbles(
        frame.camera, frame.time, Gl33BubblePass::BehindWater,
        waterSurfaceDepth);
    oceanRenderer_->composeWater(frame.camera, frame.time);
    world11DecorRenderer_->renderBubbles(
        frame.camera, frame.time, Gl33BubblePass::InFrontOfWater,
        waterSurfaceDepth);
  }
  gl.DepthMask(kTrue);
  gl.UseProgram(0);
}

void Gl33WorldRenderer::ensureInitialized() {
  if (resources_ != nullptr) {
    return;
  }
  resources_ = std::make_unique<Resources>();
  resources_->program.build(kWorldVertexShaderSource, kWorldFragmentShaderSource);
  resources_->portalProgram.build(kWorldVertexShaderSource,
                                  kPortalFragmentShaderSource);
  upload(resources_->cube, makeCube());
  upload(resources_->pyramid, makePyramid());
  upload(resources_->cylinder, makeCylinder(12));
  upload(resources_->crystal, makeCrystal());
  upload(resources_->annulus, makeAnnulus());
  upload(resources_->portalQuad, makePortalQuad());
  upload(resources_->sphere, makeSphere());
  if (worldId_ == 11) upload(resources_->reefRock, makeReefRock());
  if (worldId_ == 7 || worldId_ == 12) {
    upload(resources_->terrain, makeTerrain(worldId_));
    resources_->hasTerrain = true;
  }
  if (worldId_ == 9) {
    resources_->wallTexture.loadFget(
        "datasets/0x00000014.fget", Gl33TextureWrap::Repeat);
    resources_->panelTexture.loadFget(
        "datasets/0x00000015.fget", Gl33TextureWrap::ClampToEdge);
    resources_->hasCorridorTextures = true;
  }
}

}  // namespace hg::render::gl33
