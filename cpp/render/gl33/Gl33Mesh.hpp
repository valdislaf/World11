#pragma once

#include "render/gl33/Gl33Buffer.hpp"
#include "render/gl33/Gl33VertexArray.hpp"

#include <cstdint>
#include <vector>

namespace hg::render::gl33 {

/// <summary>Position, normal, UV and material-surface layout shared by Core meshes.</summary>
struct Gl33Vertex {
  float position[3];
  float normal[3];
  float uv[2];
  float surfaceType;
};

/// <summary>Indexed VAO/VBO/EBO mesh rendered with <c>glDrawElements</c>.</summary>
class Gl33Mesh final {
public:
  Gl33Mesh();

  void upload(const std::vector<Gl33Vertex>& vertices,
              const std::vector<std::uint32_t>& indices);
  void draw() const;

private:
  Gl33VertexArray vertexArray_;
  Gl33Buffer vertexBuffer_;
  Gl33Buffer indexBuffer_;
  SizeI indexCount_ = 0;
};

}  // namespace hg::render::gl33
