#include "render/gl33/Gl33HudRenderer.hpp"

#include "hg_runtime_frame_state.hpp"
#include "hg_runtime_input_snapshot.hpp"
#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Buffer.hpp"
#include "render/gl33/Gl33ShaderProgram.hpp"
#include "render/gl33/Gl33VertexArray.hpp"
#include "third_party/stb_easy_font.h"
#include "world/HudViewModel.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace hg::render::gl33 {

namespace {

struct HudVertex {
  float position[2];
  float color[4];
};

struct EasyFontVertex {
  float x;
  float y;
  float z;
  unsigned char color[4];
};

static_assert(sizeof(EasyFontVertex) == 16, "Unexpected stb_easy_font vertex layout");

using Matrix4 = std::array<float, 16>;

struct Resources {
  Resources() : vertexBuffer(kArrayBuffer), indexBuffer(kElementArrayBuffer) {}
  Gl33VertexArray vertexArray;
  Gl33Buffer vertexBuffer;
  Gl33Buffer indexBuffer;
  Gl33ShaderProgram shader;
};

std::unique_ptr<Resources> g_resources;
bool g_visible = true;
bool g_f1WasDown = false;

constexpr char kHudVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform mat4 uProjection;
out vec4 vColor;
void main() {
  vColor = aColor;
  gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
}
)glsl";

constexpr char kHudFragmentShader[] = R"glsl(#version 330 core
in vec4 vColor;
out vec4 fragmentColor;
void main() {
  fragmentColor = vColor;
}
)glsl";

Matrix4 pixelProjection(float width, float height) {
  return Matrix4{
      2.0f / width, 0.0f, 0.0f, 0.0f,
      0.0f, -2.0f / height, 0.0f, 0.0f,
      0.0f, 0.0f, -1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f, 1.0f,
  };
}

void addQuad(std::vector<HudVertex>& vertices,
             std::vector<std::uint32_t>& indices,
             float left, float top, float right, float bottom,
             std::array<float, 4> color) {
  const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
  vertices.push_back({{left, top}, {color[0], color[1], color[2], color[3]}});
  vertices.push_back({{right, top}, {color[0], color[1], color[2], color[3]}});
  vertices.push_back({{right, bottom}, {color[0], color[1], color[2], color[3]}});
  vertices.push_back({{left, bottom}, {color[0], color[1], color[2], color[3]}});
  indices.insert(indices.end(), {first, first + 1, first + 2,
                                 first, first + 2, first + 3});
}

void addText(std::vector<HudVertex>& vertices,
             std::vector<std::uint32_t>& indices,
             const std::string& text, float x, float y, float scale,
             std::array<unsigned char, 4> color) {
  std::array<unsigned char, 65536> rawBuffer{};
  std::vector<char> mutableText(text.begin(), text.end());
  mutableText.push_back('\0');
  const int quadCount = stb_easy_font_print(
      0.0f, 0.0f, mutableText.data(), color.data(),
      rawBuffer.data(), static_cast<int>(rawBuffer.size()));

  for (int quad = 0; quad < quadCount; ++quad) {
    const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
    for (int corner = 0; corner < 4; ++corner) {
      EasyFontVertex source{};
      const std::size_t offset = static_cast<std::size_t>(quad * 4 + corner) *
          sizeof(EasyFontVertex);
      std::memcpy(&source, rawBuffer.data() + offset, sizeof(source));
      vertices.push_back({
          {x + source.x * scale, y + source.y * scale},
          {source.color[0] / 255.0f, source.color[1] / 255.0f,
           source.color[2] / 255.0f, source.color[3] / 255.0f},
      });
    }
    indices.insert(indices.end(), {first, first + 1, first + 2,
                                   first, first + 2, first + 3});
  }
}

void ensureInitialized() {
  if (g_resources != nullptr) {
    return;
  }
  g_resources = std::make_unique<Resources>();
  g_resources->shader.build(kHudVertexShader, kHudFragmentShader);
  g_resources->vertexArray.bind();
  g_resources->vertexBuffer.bind();
  api().EnableVertexAttribArray(0);
  api().VertexAttribPointer(0, 2, kFloat, kFalse,
      static_cast<SizeI>(sizeof(HudVertex)),
      reinterpret_cast<const void*>(offsetof(HudVertex, position)));
  api().EnableVertexAttribArray(1);
  api().VertexAttribPointer(1, 4, kFloat, kFalse,
      static_cast<SizeI>(sizeof(HudVertex)),
      reinterpret_cast<const void*>(offsetof(HudVertex, color)));
  api().BindVertexArray(0);
  api().BindBuffer(kArrayBuffer, 0);
}

}  // namespace

void Gl33HudRenderer::render() {
  const bool f1Down = RuntimeInputSnapshot::keyState(GLFW_KEY_F1) == GLFW_PRESS;
  if (f1Down && !g_f1WasDown) {
    g_visible = !g_visible;
  }
  g_f1WasDown = f1Down;
  if (!g_visible) {
    return;
  }

  const int width = RuntimeFrameState::framebufferWidth();
  const int height = RuntimeFrameState::framebufferHeight();
  if (width <= 0 || height <= 0) {
    return;
  }
  ensureInitialized();

  const float uiScale = std::clamp(static_cast<float>(height) / 1000.0f, 0.75f, 2.0f);
  std::vector<HudVertex> vertices;
  std::vector<std::uint32_t> indices;
  addQuad(vertices, indices, 10.0f * uiScale, 10.0f * uiScale,
          440.0f * uiScale, 142.0f * uiScale,
          {0.035f, 0.055f, 0.070f, 0.72f});

  const world::HudViewModel model = world::HudViewModel::create();
  constexpr float kFontScale = 1.25f;
  const auto addLine = [&](const std::string& value, float y,
                           std::array<unsigned char, 4> color) {
    addText(vertices, indices, value, 16.0f * uiScale, y * uiScale,
            kFontScale * uiScale, color);
  };
  addLine(model.speedText, 24.0f, {255, 255, 255, 255});
  addLine(model.localCameraText, 46.0f, {210, 220, 225, 255});
  addLine(model.globalCameraText, 68.0f, {175, 225, 255, 255});
  addLine(model.worldDistanceText, 90.0f, {180, 255, 190, 255});
  addLine(model.worldDistanceUnitsText, 112.0f, {155, 225, 165, 255});

  g_resources->vertexArray.bind();
  g_resources->vertexBuffer.upload(vertices.data(),
      static_cast<SizePtr>(vertices.size() * sizeof(HudVertex)), kDynamicDraw);
  g_resources->indexBuffer.upload(indices.data(),
      static_cast<SizePtr>(indices.size() * sizeof(std::uint32_t)), kDynamicDraw);

  Gl33Api& gl = api();
  gl.Disable(kDepthTest);
  gl.Enable(kBlend);
  gl.BlendFunc(kSourceAlpha, kOneMinusSourceAlpha);
  gl.DepthMask(kFalse);
  g_resources->shader.use();
  const Matrix4 projection = pixelProjection(static_cast<float>(width),
                                             static_cast<float>(height));
  g_resources->shader.setMatrix4("uProjection", projection.data());
  gl.DrawElements(kTriangles, static_cast<SizeI>(indices.size()),
                  kUnsignedInt, nullptr);
  gl.BindVertexArray(0);
  gl.UseProgram(0);
  gl.DepthMask(kTrue);
  gl.Disable(kBlend);
  gl.Enable(kDepthTest);
}

void Gl33HudRenderer::reset() {
  g_resources.reset();
  g_visible = true;
  g_f1WasDown = false;
}

}  // namespace hg::render::gl33
