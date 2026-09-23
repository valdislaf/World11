#include "render/gl33/Gl33Skybox.hpp"

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Buffer.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33Texture.hpp"
#include "render/gl33/Gl33VertexArray.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;

struct Vec3 {
  float x;
  float y;
  float z;
};

struct SkyboxVertex {
  float position[3];
  float uv[2];
};

struct UvRect {
  float u0;
  float v0;
  float u1;
  float v1;
};

using Matrix4 = std::array<float, 16>;

float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return Vec3{lhs.y * rhs.z - lhs.z * rhs.y,
              lhs.z * rhs.x - lhs.x * rhs.z,
              lhs.x * rhs.y - lhs.y * rhs.x};
}

Vec3 normalize(const Vec3& value) {
  const float length = std::sqrt(dot(value, value));
  if (length <= 1.0e-6f) {
    return Vec3{0.0f, 1.0f, 0.0f};
  }
  return Vec3{value.x / length, value.y / length, value.z / length};
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

constexpr UvRect atlasRect(float column, float row) {
  return UvRect{column / 4.0f, row / 3.0f,
                (column + 1.0f) / 4.0f, (row + 1.0f) / 3.0f};
}

UvRect inset(UvRect rect, int width, int height) {
  const float du = 1.0f / static_cast<float>(std::max(width, 1));
  const float dv = 1.0f / static_cast<float>(std::max(height, 1));
  rect.u0 += du;
  rect.u1 -= du;
  rect.v0 += dv;
  rect.v1 -= dv;
  return rect;
}

UvRect upperPart(const UvRect& face, float sourceHorizon) {
  return UvRect{face.u0, face.v0, face.u1,
                face.v0 + (face.v1 - face.v0) * sourceHorizon};
}

UvRect lowerPart(const UvRect& face, float sourceHorizon) {
  return UvRect{face.u0,
                face.v0 + (face.v1 - face.v0) * sourceHorizon,
                face.u1, face.v1};
}

void addQuad(std::vector<SkyboxVertex>& vertices,
             std::vector<std::uint32_t>& indices, const UvRect& uv,
             Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
  const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
  vertices.push_back({{a.x, a.y, a.z}, {uv.u0, uv.v1}});
  vertices.push_back({{b.x, b.y, b.z}, {uv.u1, uv.v1}});
  vertices.push_back({{c.x, c.y, c.z}, {uv.u1, uv.v0}});
  vertices.push_back({{d.x, d.y, d.z}, {uv.u0, uv.v0}});
  indices.insert(indices.end(), {first, first + 1, first + 2,
                                 first, first + 2, first + 3});
}

void buildAtlasCube(int width, int height, float sourceHorizon,
                    float targetHorizon, std::vector<SkyboxVertex>& vertices,
                    std::vector<std::uint32_t>& indices) {
  const UvRect top = inset(atlasRect(1.0f, 0.0f), width, height);
  const UvRect left = inset(atlasRect(0.0f, 1.0f), width, height);
  const UvRect front = inset(atlasRect(1.0f, 1.0f), width, height);
  const UvRect right = inset(atlasRect(2.0f, 1.0f), width, height);
  const UvRect back = inset(atlasRect(3.0f, 1.0f), width, height);
  const UvRect bottom = inset(atlasRect(1.0f, 2.0f), width, height);
  const float horizonY = 1.0f - 2.0f * targetHorizon;

  addQuad(vertices, indices, lowerPart(front, sourceHorizon),
          {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
          {1.0f, horizonY, -1.0f}, {-1.0f, horizonY, -1.0f});
  addQuad(vertices, indices, upperPart(front, sourceHorizon),
          {-1.0f, horizonY, -1.0f}, {1.0f, horizonY, -1.0f},
          {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f});
  addQuad(vertices, indices, lowerPart(back, sourceHorizon),
          {1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f},
          {-1.0f, horizonY, 1.0f}, {1.0f, horizonY, 1.0f});
  addQuad(vertices, indices, upperPart(back, sourceHorizon),
          {1.0f, horizonY, 1.0f}, {-1.0f, horizonY, 1.0f},
          {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f});
  addQuad(vertices, indices, lowerPart(left, sourceHorizon),
          {-1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f},
          {-1.0f, horizonY, -1.0f}, {-1.0f, horizonY, 1.0f});
  addQuad(vertices, indices, upperPart(left, sourceHorizon),
          {-1.0f, horizonY, 1.0f}, {-1.0f, horizonY, -1.0f},
          {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f});
  addQuad(vertices, indices, lowerPart(right, sourceHorizon),
          {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f},
          {1.0f, horizonY, 1.0f}, {1.0f, horizonY, -1.0f});
  addQuad(vertices, indices, upperPart(right, sourceHorizon),
          {1.0f, horizonY, -1.0f}, {1.0f, horizonY, 1.0f},
          {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, -1.0f});
  addQuad(vertices, indices, top,
          {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
          {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
  addQuad(vertices, indices, bottom,
          {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
          {1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f});
}

constexpr char kSkyboxVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUv;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uVerticalOffset;
out vec2 vUv;
out vec3 vDirection;
void main() {
  vUv = aUv;
  vDirection = aPosition;
  vec3 direction = aPosition;
  direction.y += uVerticalOffset;
  vec4 clip = uProjection * uView * vec4(direction, 1.0);
  gl_Position = clip.xyww;
}
)glsl";

constexpr char kSkyboxFragmentShader[] = R"glsl(#version 330 core
in vec2 vUv;
in vec3 vDirection;
uniform sampler2D uAtlas;
uniform int uUnderwaterMask;
uniform float uCameraY;
uniform float uWaterSurfaceY;
uniform vec3 uUnderwaterFogColor;
out vec4 fragmentColor;
void main() {
  vec3 atlasColor = texture(uAtlas, vUv).rgb;
  if (uUnderwaterMask != 0 && uCameraY < uWaterSurfaceY) {
    float upwardRay = normalize(vDirection).y;
    float skyVisibility = smoothstep(0.035, 0.16, upwardRay);
    fragmentColor = vec4(mix(uUnderwaterFogColor, atlasColor, skyVisibility), 1.0);
    return;
  }
  fragmentColor = vec4(atlasColor, 1.0);
}
)glsl";

}  // namespace

struct Gl33Skybox::Resources {
  Resources() : vertexBuffer(kArrayBuffer), indexBuffer(kElementArrayBuffer) {}
  Gl33Texture texture;
  Gl33VertexArray vertexArray;
  Gl33Buffer vertexBuffer;
  Gl33Buffer indexBuffer;
  Gl33ShaderProgram shader;
  SizeI indexCount = 0;
};

Gl33Skybox::Gl33Skybox(std::string fgetPath, float radius,
                       float sourceSideHorizon, float targetSideHorizon,
                       bool underwaterMask, float waterSurfaceY,
                       std::array<float, 3> underwaterFogColor)
    : fgetPath_(std::move(fgetPath)), radius_(radius),
      sourceSideHorizon_(std::clamp(sourceSideHorizon, 0.01f, 0.99f)),
      targetSideHorizon_(std::clamp(targetSideHorizon, 0.01f, 0.99f)),
      underwaterMask_(underwaterMask), waterSurfaceY_(waterSurfaceY),
      underwaterFogColor_(underwaterFogColor) {
}

Gl33Skybox::~Gl33Skybox() = default;

void Gl33Skybox::render(const Gl33Camera& camera, float centerY) {
  ensureInitialized();
  const Vec3 eye{camera.position[0], camera.position[1], camera.position[2]};
  const Vec3 front{camera.front[0], camera.front[1], camera.front[2]};
  const Vec3 up{camera.up[0], camera.up[1], camera.up[2]};
  const float aspect = static_cast<float>(std::max(camera.framebufferWidth, 1)) /
      static_cast<float>(std::max(camera.framebufferHeight, 1));
  const Matrix4 viewMatrix = view({0.0f, 0.0f, 0.0f}, front, up);
  const Matrix4 projection = perspective(60.0f * kPi / 180.0f, aspect, 0.1f, 520.0f);

  Gl33Api& gl = api();
  gl.Disable(kBlend);
  gl.DepthMask(kFalse);
  resources_->shader.use();
  resources_->shader.setMatrix4("uView", viewMatrix.data());
  resources_->shader.setMatrix4("uProjection", projection.data());
  resources_->shader.setFloat("uVerticalOffset", (centerY - eye.y) / radius_);
  resources_->shader.setInt("uUnderwaterMask", underwaterMask_ ? 1 : 0);
  resources_->shader.setFloat("uCameraY", eye.y);
  resources_->shader.setFloat("uWaterSurfaceY", waterSurfaceY_);
  resources_->shader.setVec3("uUnderwaterFogColor", underwaterFogColor_[0],
                             underwaterFogColor_[1], underwaterFogColor_[2]);
  resources_->shader.setInt("uAtlas", 0);
  resources_->texture.bind(0);
  resources_->vertexArray.bind();
  gl.DrawElements(kTriangles, resources_->indexCount, kUnsignedInt, nullptr);
  gl.BindVertexArray(0);
  gl.BindTexture(kTexture2D, 0);
  gl.DepthMask(kTrue);
  gl.UseProgram(0);
}

void Gl33Skybox::ensureInitialized() {
  if (resources_ != nullptr) {
    return;
  }
  resources_ = std::make_unique<Resources>();
  resources_->texture.loadFget(fgetPath_);
  resources_->shader.build(kSkyboxVertexShader, kSkyboxFragmentShader);

  std::vector<SkyboxVertex> vertices;
  std::vector<std::uint32_t> indices;
  buildAtlasCube(resources_->texture.width(), resources_->texture.height(),
                 sourceSideHorizon_, targetSideHorizon_, vertices, indices);
  resources_->vertexArray.bind();
  resources_->vertexBuffer.upload(vertices.data(),
      static_cast<SizePtr>(vertices.size() * sizeof(SkyboxVertex)));
  resources_->indexBuffer.upload(indices.data(),
      static_cast<SizePtr>(indices.size() * sizeof(std::uint32_t)));
  api().EnableVertexAttribArray(0);
  api().VertexAttribPointer(0, 3, kFloat, kFalse,
      static_cast<SizeI>(sizeof(SkyboxVertex)),
      reinterpret_cast<const void*>(offsetof(SkyboxVertex, position)));
  api().EnableVertexAttribArray(1);
  api().VertexAttribPointer(1, 2, kFloat, kFalse,
      static_cast<SizeI>(sizeof(SkyboxVertex)),
      reinterpret_cast<const void*>(offsetof(SkyboxVertex, uv)));
  api().BindVertexArray(0);
  api().BindBuffer(kArrayBuffer, 0);
  resources_->indexCount = static_cast<SizeI>(indices.size());
}

}  // namespace hg::render::gl33
