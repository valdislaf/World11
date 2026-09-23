#include "render/gl33/Gl33Mesh.hpp"

#include <cstddef>

namespace hg::render::gl33 {

Gl33Mesh::Gl33Mesh()
    : vertexBuffer_(kArrayBuffer), indexBuffer_(kElementArrayBuffer) {
}

void Gl33Mesh::upload(const std::vector<Gl33Vertex>& vertices,
                      const std::vector<std::uint32_t>& indices) {
  vertexArray_.bind();
  vertexBuffer_.upload(vertices.data(),
      static_cast<SizePtr>(vertices.size() * sizeof(Gl33Vertex)));
  indexBuffer_.upload(indices.data(),
      static_cast<SizePtr>(indices.size() * sizeof(std::uint32_t)));

  api().EnableVertexAttribArray(0);
  api().VertexAttribPointer(0, 3, kFloat, kFalse,
      static_cast<SizeI>(sizeof(Gl33Vertex)),
      reinterpret_cast<const void*>(offsetof(Gl33Vertex, position)));
  api().EnableVertexAttribArray(1);
  api().VertexAttribPointer(1, 3, kFloat, kFalse,
      static_cast<SizeI>(sizeof(Gl33Vertex)),
      reinterpret_cast<const void*>(offsetof(Gl33Vertex, normal)));
  api().EnableVertexAttribArray(2);
  api().VertexAttribPointer(2, 2, kFloat, kFalse,
      static_cast<SizeI>(sizeof(Gl33Vertex)),
      reinterpret_cast<const void*>(offsetof(Gl33Vertex, uv)));
  api().EnableVertexAttribArray(3);
  api().VertexAttribPointer(3, 1, kFloat, kFalse,
      static_cast<SizeI>(sizeof(Gl33Vertex)),
      reinterpret_cast<const void*>(offsetof(Gl33Vertex, surfaceType)));

  api().BindVertexArray(0);
  api().BindBuffer(kArrayBuffer, 0);
  indexCount_ = static_cast<SizeI>(indices.size());
}

void Gl33Mesh::draw() const {
  vertexArray_.bind();
  api().DrawElements(kTriangles, indexCount_, kUnsignedInt, nullptr);
  api().BindVertexArray(0);
}

}  // namespace hg::render::gl33
