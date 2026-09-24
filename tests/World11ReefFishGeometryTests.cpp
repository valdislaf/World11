// Run from the repository root (CTest sets the working directory) so the
// packed texture in datasets/ can be checked against the mesh profile.
#include "render/gl33/World11ReefFishGeometry.hpp"
#include "world/World11ReefFishSchool.hpp"
#include "world/World11Seabed.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using hg::render::gl33::ReefFishMeshData;
using hg::render::gl33::ReefFishVertex;

constexpr float kHalfWidth = 0.40f;
constexpr float kHalfHeight = 0.20f;

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "World11ReefFishGeometry test failed: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 position(const ReefFishVertex& vertex) {
  return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

void testProfile() {
  for (int i = 0; i <= 100; ++i) {
    const float u = hg::render::gl33::kReefFishBodyStartU +
        (hg::render::gl33::kReefFishBodyEndU -
         hg::render::gl33::kReefFishBodyStartU) * static_cast<float>(i) / 100.0f;
    const auto edges = hg::render::gl33::reefFishBodyEdges(u);
    if (!(edges.top < edges.bottom) || edges.top < 0.0f || edges.bottom > 1.0f) {
      fail("body outline must satisfy 0 <= top < bottom <= 1");
    }
    if (u >= 0.30f && u <= 0.80f &&
        hg::render::gl33::reefFishDorsalEdge(u) >= edges.top) {
      fail("dorsal fin must rise above the body");
    }
    if (u >= 0.50f && u <= 0.80f &&
        hg::render::gl33::reefFishVentralEdge(u) <= edges.bottom) {
      fail("anal fin must extend below the body");
    }
    const float ratio = hg::render::gl33::reefFishThicknessRatio(u);
    if (ratio <= 0.1f || ratio >= 0.7f) {
      fail("body must stay laterally compressed");
    }
  }
}

void testMesh() {
  const ReefFishMeshData mesh =
      hg::render::gl33::buildReefFishMesh(kHalfWidth, kHalfHeight);
  if (mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.indices.size() % 3 != 0) {
    fail("mesh must contain whole triangles");
  }
  for (const ReefFishVertex& vertex : mesh.vertices) {
    const Vec3 n{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
    if (std::abs(std::sqrt(dot(n, n)) - 1.0f) > 1.0e-3f) {
      fail("normals must be unit length");
    }
    if (vertex.uv[0] < 0.0f || vertex.uv[0] > 1.0f ||
        vertex.uv[1] < 0.0f || vertex.uv[1] > 1.0f) {
      fail("texture coordinates must stay inside the atlas");
    }
    if (std::abs(vertex.position[0]) > kHalfWidth + 1.0e-4f ||
        std::abs(vertex.position[1]) > kHalfHeight + 1.0e-4f) {
      fail("vertex outside the texture rectangle");
    }
    if (vertex.surfaceType != hg::render::gl33::kReefFishBodySurface &&
        vertex.surfaceType != hg::render::gl33::kReefFishFinSurface) {
      fail("unknown surface type");
    }
  }

  std::map<std::pair<std::uint32_t, std::uint32_t>, int> bodyEdges;
  std::size_t bodyTriangles = 0;
  std::size_t finTriangles = 0;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    const std::uint32_t ids[3] = {
        mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]};
    for (const std::uint32_t id : ids) {
      if (id >= mesh.vertices.size()) {
        fail("index out of range");
      }
    }
    const ReefFishVertex& a = mesh.vertices[ids[0]];
    const ReefFishVertex& b = mesh.vertices[ids[1]];
    const ReefFishVertex& c = mesh.vertices[ids[2]];
    if (a.surfaceType != b.surfaceType || a.surfaceType != c.surfaceType) {
      fail("triangle mixes body and fin vertices");
    }
    const Vec3 face = cross(sub(position(b), position(a)),
                            sub(position(c), position(a)));
    const Vec3 normalSum{a.normal[0] + b.normal[0] + c.normal[0],
                         a.normal[1] + b.normal[1] + c.normal[1],
                         a.normal[2] + b.normal[2] + c.normal[2]};
    // The fish shader discards back faces, so winding must match normals.
    if (dot(face, normalSum) < 0.0f) {
      fail("triangle winding disagrees with its normals");
    }
    if (a.surfaceType == hg::render::gl33::kReefFishBodySurface) {
      ++bodyTriangles;
      for (int edge = 0; edge < 3; ++edge) {
        const std::uint32_t from = ids[edge];
        const std::uint32_t to = ids[(edge + 1) % 3];
        ++bodyEdges[{std::min(from, to), std::max(from, to)}];
      }
    } else {
      ++finTriangles;
    }
  }
  for (const auto& edge : bodyEdges) {
    if (edge.second != 2) {
      fail("body must be a closed, watertight surface");
    }
  }
  if (bodyTriangles < 2000 || finTriangles < 100 || finTriangles % 2 != 0) {
    fail("expected a dense body and double-sided fins");
  }

  const ReefFishMeshData again =
      hg::render::gl33::buildReefFishMesh(kHalfWidth, kHalfHeight);
  if (again.indices != mesh.indices ||
      again.vertices.size() != mesh.vertices.size() ||
      std::memcmp(again.vertices.data(), mesh.vertices.data(),
                  mesh.vertices.size() * sizeof(ReefFishVertex)) != 0) {
    fail("mesh generation must be deterministic");
  }
}

std::vector<unsigned char> readFget(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),
                                   std::istreambuf_iterator<char>());
  if (bytes.size() <= 5 || std::memcmp(bytes.data(), "FGET", 4) != 0) {
    fail(std::string("cannot read FGET asset ") + path);
  }
  const unsigned char key = bytes[4];
  std::vector<unsigned char> png(bytes.begin() + 5, bytes.end());
  for (unsigned char& value : png) {
    value ^= key;
  }
  return png;
}

void testTexture() {
  const std::vector<unsigned char> png = readFget("datasets/0x0000001C.fget");
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* pixels = stbi_load_from_memory(
      png.data(), static_cast<int>(png.size()), &width, &height, &channels, 4);
  if (pixels == nullptr || width != 1024 || height != 512) {
    fail("reef fish texture must decode as a 1024x512 PNG");
  }
  const auto alphaAt = [&](float u, float v) {
    const int x = std::min(width - 1, static_cast<int>(u * width));
    const int y = std::min(height - 1, static_cast<int>(v * height));
    return pixels[(y * width + x) * 4 + 3];
  };
  // The painted body must follow the same outline as the mesh.
  for (int i = 1; i < 20; ++i) {
    const float u = 0.06f + 0.74f * static_cast<float>(i) / 20.0f;
    const auto edges = hg::render::gl33::reefFishBodyEdges(u);
    const float inset = 0.02f;
    if (alphaAt(u, edges.top + inset) < 250 ||
        alphaAt(u, 0.5f * (edges.top + edges.bottom)) < 250 ||
        alphaAt(u, edges.bottom - inset) < 250) {
      fail("texture body alpha does not match the mesh outline");
    }
  }
  // Snout region above and below the body is empty (no fins there).
  const auto snout = hg::render::gl33::reefFishBodyEdges(0.12f);
  if (alphaAt(0.12f, snout.top - 0.03f) != 0 ||
      alphaAt(0.12f, snout.bottom + 0.03f) != 0) {
    fail("texture must be transparent around the snout");
  }
  // Pectoral atlas contains an opaque-enough fin.
  const float* atlas = hg::render::gl33::kReefFishPectoralAtlas;
  if (alphaAt(0.5f * (atlas[0] + atlas[1]), 0.5f * (atlas[2] + atlas[3])) < 128) {
    fail("pectoral fin atlas is missing");
  }
  stbi_image_free(pixels);
}

void testSchool() {
  for (std::size_t i = 0; i < hg::world::kWorld11ReefFishCount; ++i) {
    hg::world::World11FishTrajectory trajectory =
        hg::world::world11ReefFishTrajectory(i);
    const auto& volume = trajectory.movementVolume();
    for (int step = 0; step <= 600; ++step) {
      const auto state = trajectory.sample(step * 0.25);
      const float seabed = hg::world::world11SeabedHeight(
          state.position.x, state.position.z);
      if (state.position.y > seabed + volume.maximumSeabedHeight + 2.0e-3f) {
        fail("reef fish rose above its seabed ceiling");
      }
      if (state.position.y - volume.fishHalfHeight <
          seabed + volume.seabedClearance - 2.0e-3f) {
        fail("reef fish touched the seabed");
      }
    }
  }
}

}  // namespace

int main() {
  testProfile();
  testMesh();
  testTexture();
  testSchool();
  std::cout << "World11ReefFishGeometry tests passed\n";
  return EXIT_SUCCESS;
}
