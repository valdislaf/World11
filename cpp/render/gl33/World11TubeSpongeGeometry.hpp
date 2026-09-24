#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace hg::render::gl33 {

/// <summary>One sponge tube of the World 11 colony landmark, in world space.</summary>
struct TubeSpongeTube {
  std::array<float, 3> base;
  /// <summary>Unit growth direction at the base; tubes curve towards vertical.</summary>
  std::array<float, 3> direction;
  float height;
  float radius;
  float wallThickness;
  std::uint32_t seed;
};

/// <summary>Vertex layout identical to <c>Gl33Vertex</c>.</summary>
struct TubeSpongeVertex {
  float position[3];
  float normal[3];
  float uv[2];
  float surfaceType;
};

struct TubeSpongeMeshData {
  std::vector<TubeSpongeVertex> vertices;
  std::vector<std::uint32_t> indices;
};

/// <summary>
/// Colony geometry split by material: the textured outer skin with its
/// rounded rim, and the shaded interior cavity with its floor.
/// </summary>
struct TubeSpongeColonyMesh {
  TubeSpongeMeshData outer;
  TubeSpongeMeshData inner;
};

/// <summary>Texture repeat length along and around a tube, metres.</summary>
inline constexpr float kTubeSpongeTextureTile = 1.0f;
/// <summary>The cavity floor sits at this fraction of the tube height.</summary>
inline constexpr float kTubeSpongeCavityFloor = 0.25f;

/// <summary>
/// Deterministic tube layout of the colony around <c>kWorld11Colony</c>.
/// Bases are sunk slightly into the seabed.
/// </summary>
std::vector<TubeSpongeTube> world11TubeSpongeColony();

/// <summary>Centre of <paramref name="tube"/>'s axis at parameter t (0 base, 1 rim).</summary>
std::array<float, 3> tubeSpongeCenter(const TubeSpongeTube& tube, float t);

/// <summary>
/// Builds knobbly hollow tubes: outer wall and rim in <c>outer</c>, inner wall
/// and cavity floor in <c>inner</c>. UVs tile <c>kTubeSpongeTextureTile</c>.
/// </summary>
TubeSpongeColonyMesh buildTubeSpongeColonyMesh(
    const std::vector<TubeSpongeTube>& tubes);

}  // namespace hg::render::gl33
