#include "render/gl33/World11TubeSpongeGeometry.hpp"
#include "world/World11Landmarks.hpp"
#include "world/World11Seabed.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using hg::render::gl33::TubeSpongeMeshData;
using hg::render::gl33::TubeSpongeTube;
using hg::render::gl33::TubeSpongeVertex;

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "World11TubeSpongeGeometry test failed: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 position(const TubeSpongeVertex& vertex) {
  return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

Vec3 normal(const TubeSpongeVertex& vertex) {
  return {vertex.normal[0], vertex.normal[1], vertex.normal[2]};
}

void validateMesh(const TubeSpongeMeshData& mesh, const char* name) {
  if (mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.indices.size() % 3 != 0) {
    fail(std::string(name) + " mesh must contain whole triangles");
  }
  for (const TubeSpongeVertex& vertex : mesh.vertices) {
    const Vec3 n = normal(vertex);
    if (std::abs(std::sqrt(dot(n, n)) - 1.0f) > 1.0e-3f) {
      fail(std::string(name) + " normals must be unit length");
    }
    if (!std::isfinite(vertex.uv[0]) || !std::isfinite(vertex.uv[1])) {
      fail(std::string(name) + " texture coordinates must be finite");
    }
  }
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    for (std::size_t k = 0; k < 3; ++k) {
      if (mesh.indices[i + k] >= mesh.vertices.size()) {
        fail(std::string(name) + " index out of range");
      }
    }
    const TubeSpongeVertex& a = mesh.vertices[mesh.indices[i]];
    const TubeSpongeVertex& b = mesh.vertices[mesh.indices[i + 1]];
    const TubeSpongeVertex& c = mesh.vertices[mesh.indices[i + 2]];
    const Vec3 face = cross(sub(position(b), position(a)),
                            sub(position(c), position(a)));
    const Vec3 sum{a.normal[0] + b.normal[0] + c.normal[0],
                   a.normal[1] + b.normal[1] + c.normal[1],
                   a.normal[2] + b.normal[2] + c.normal[2]};
    if (dot(face, sum) < 0.0f) {
      fail(std::string(name) + " triangle winding disagrees with normals");
    }
  }
}

/// Fraction of vertices whose normal points away from (sign > 0) or towards
/// (sign < 0) the nearest sample of this tube's own axis.
float radialAgreement(const TubeSpongeMeshData& mesh,
                      const TubeSpongeTube& tube, float sign) {
  std::size_t agreeing = 0;
  for (const TubeSpongeVertex& vertex : mesh.vertices) {
    const Vec3 p = position(vertex);
    float best = 1.0e9f;
    Vec3 axisPoint{0, 0, 0};
    for (int step = -3; step <= 50; ++step) {
      const auto c = hg::render::gl33::tubeSpongeCenter(tube, step / 50.0f);
      const Vec3 center{c[0], c[1], c[2]};
      const Vec3 d = sub(p, center);
      const float distance = dot(d, d);
      if (distance < best) {
        best = distance;
        axisPoint = center;
      }
    }
    if (sign * dot(normal(vertex), sub(p, axisPoint)) > 0.0f) {
      ++agreeing;
    }
  }
  return static_cast<float>(agreeing) / static_cast<float>(mesh.vertices.size());
}

}  // namespace

int main() {
  const std::vector<TubeSpongeTube> tubes =
      hg::render::gl33::world11TubeSpongeColony();
  if (tubes.size() < 12) {
    fail("colony must contain several clumps of tubes");
  }
  const auto colony = hg::world::kWorld11Colony;
  for (std::size_t i = 0; i < tubes.size(); ++i) {
    const TubeSpongeTube& tube = tubes[i];
    const float seabed = hg::world::world11SeabedHeight(tube.base[0], tube.base[2]);
    if (tube.base[1] > seabed || tube.base[1] < seabed - 0.5f) {
      fail("tube foot must be sunk just below the seabed");
    }
    if (std::hypot(tube.base[0] - colony.x, tube.base[2] - colony.z) > 4.5f) {
      fail("tube grows away from the colony landmark");
    }
    if (tube.direction[1] < 0.8f || tube.height < 1.0f ||
        tube.wallThickness >= tube.radius) {
      fail("tube must grow upwards and stay hollow");
    }
    const auto top = hg::render::gl33::tubeSpongeCenter(tube, 1.0f);
    if (top[1] - seabed < 0.9f) {
      fail("tube rim must stand clearly above the seabed");
    }
    for (std::size_t j = 0; j < i; ++j) {
      const auto other = hg::render::gl33::tubeSpongeCenter(tubes[j], 1.0f);
      const float distance = std::hypot(top[0] - other[0], top[1] - other[1],
                                        top[2] - other[2]);
      if (distance < tube.radius + tubes[j].radius) {
        fail("tube openings must not intersect");
      }
    }
  }

  const auto mesh = hg::render::gl33::buildTubeSpongeColonyMesh(tubes);
  validateMesh(mesh.outer, "outer");
  validateMesh(mesh.inner, "inner");
  // Knobs tilt individual normals, but each skin must face away from its
  // own axis and each cavity wall towards it.
  for (const TubeSpongeTube& tube : tubes) {
    const auto single = hg::render::gl33::buildTubeSpongeColonyMesh({tube});
    if (radialAgreement(single.outer, tube, 1.0f) < 0.85f) {
      fail("outer skin normals must point away from the tube axis");
    }
    if (radialAgreement(single.inner, tube, -1.0f) < 0.85f) {
      fail("cavity normals must point towards the tube axis");
    }
  }

  const auto again = hg::render::gl33::buildTubeSpongeColonyMesh(
      hg::render::gl33::world11TubeSpongeColony());
  if (again.outer.indices != mesh.outer.indices ||
      again.outer.vertices.size() != mesh.outer.vertices.size() ||
      std::memcmp(again.outer.vertices.data(), mesh.outer.vertices.data(),
                  mesh.outer.vertices.size() * sizeof(TubeSpongeVertex)) != 0) {
    fail("colony generation must be deterministic");
  }
  std::cout << "World11TubeSpongeGeometry tests passed\n";
  return EXIT_SUCCESS;
}
