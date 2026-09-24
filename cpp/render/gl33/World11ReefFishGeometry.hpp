#pragma once

#include <cstdint>
#include <vector>

namespace hg::render::gl33 {

/// <summary>Surface kinds matching <c>aSurfaceType</c> in the fish shader.</summary>
inline constexpr float kReefFishBodySurface = 0.0f;
inline constexpr float kReefFishFinSurface = 1.0f;

/// <summary>Texture-space extent of the lofted body (u = 0 is the snout).</summary>
inline constexpr float kReefFishBodyStartU = 0.030f;
inline constexpr float kReefFishBodyEndU = 0.840f;
/// <summary>Texture-space extent of the caudal fin.</summary>
inline constexpr float kReefFishTailStartU = 0.820f;
inline constexpr float kReefFishTailEndU = 0.975f;
/// <summary>Pectoral-fin atlas in the texture: u0, u1, v0, v1.</summary>
inline constexpr float kReefFishPectoralAtlas[4] = {0.020f, 0.180f, 0.840f, 0.980f};

/// <summary>Vertex layout identical to <c>Gl33Vertex</c>.</summary>
struct ReefFishVertex {
  float position[3];
  float normal[3];
  float uv[2];
  float surfaceType;
};

struct ReefFishMeshData {
  std::vector<ReefFishVertex> vertices;
  std::vector<std::uint32_t> indices;
};

/// <summary>Top and bottom of the body outline in texture v (0 = top).</summary>
struct ReefFishBodyEdges {
  float top;
  float bottom;
};

/// <summary>
/// Body outline of the reef butterflyfish at texture u. The knot table is
/// shared with scripts/generate_fish_02.py so the painted body matches the mesh.
/// </summary>
ReefFishBodyEdges reefFishBodyEdges(float u);
/// <returns>Outer edge of the dorsal fin in texture v.</returns>
float reefFishDorsalEdge(float u);
/// <returns>Outer edge of the anal fin in texture v.</returns>
float reefFishVentralEdge(float u);
/// <returns>Half thickness of the body relative to its half height.</returns>
float reefFishThicknessRatio(float u);

/// <summary>
/// Builds the reef fish in local fish space: +x forward (snout), +y up,
/// +z to the side. x = halfWidth (1 - 2u), y = halfHeight (1 - 2v), matching
/// the texture mapping of the World 11 fish shader. The body is a closed
/// lofted ellipse; dorsal, anal, caudal and pectoral fins are double-sided
/// alpha-cutout surfaces.
/// </summary>
ReefFishMeshData buildReefFishMesh(float halfWidth, float halfHeight);

}  // namespace hg::render::gl33
