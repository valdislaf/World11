#include "render/gl33/World11ReefFishGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace hg::render::gl33 {

namespace {

constexpr float kTwoPi = 6.28318530718f;

// Keep these tables identical to BODY_KNOTS, DORSAL_KNOTS and VENTRAL_KNOTS
// in scripts/generate_fish_02.py.
constexpr std::array<float, 11> kBodyU = {
    0.030f, 0.060f, 0.120f, 0.200f, 0.300f, 0.420f,
    0.540f, 0.640f, 0.720f, 0.790f, 0.840f};
constexpr std::array<float, 11> kBodyTop = {
    0.505f, 0.455f, 0.360f, 0.255f, 0.180f, 0.150f,
    0.175f, 0.245f, 0.345f, 0.425f, 0.440f};
constexpr std::array<float, 11> kBodyBottom = {
    0.535f, 0.575f, 0.650f, 0.740f, 0.805f, 0.830f,
    0.800f, 0.735f, 0.640f, 0.570f, 0.560f};
constexpr std::array<float, 8> kDorsalU = {
    0.250f, 0.320f, 0.420f, 0.550f, 0.660f, 0.740f, 0.800f, 0.840f};
constexpr std::array<float, 8> kDorsalV = {
    0.215f, 0.100f, 0.055f, 0.060f, 0.110f, 0.200f, 0.330f, 0.430f};
constexpr std::array<float, 6> kVentralU = {
    0.460f, 0.520f, 0.620f, 0.720f, 0.790f, 0.840f};
constexpr std::array<float, 6> kVentralV = {
    0.820f, 0.900f, 0.925f, 0.860f, 0.720f, 0.575f};
// Lowest point of the pelvic spine painted under the chest.
constexpr float kPelvicLowestV = 0.905f;
// Fin strips reach this far into the body so no gap shows at the fin base.
constexpr float kFinOverlap = 0.035f;

constexpr int kBodySections = 64;
constexpr int kBodyRingSegments = 24;
constexpr int kFinStripSteps = 36;
constexpr int kTailSteps = 14;

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 operator*(const Vec3& value, float factor) {
  return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(const Vec3& lhs, const Vec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

Vec3 normalize(const Vec3& value, const Vec3& fallback) {
  const float length = std::sqrt(dot(value, value));
  if (length < 1.0e-8f) {
    return fallback;
  }
  return value * (1.0f / length);
}

float mix(float a, float b, float t) {
  return a + (b - a) * t;
}

float smoothstep(float edge0, float edge1, float value) {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/// Piecewise cubic Hermite with Catmull-Rom tangents and clamped ends;
/// mirrors hermite() in scripts/generate_fish_02.py.
template <std::size_t N>
float hermite(const std::array<float, N>& us, const std::array<float, N>& vs,
              float u) {
  if (u <= us.front()) {
    return vs.front();
  }
  if (u >= us.back()) {
    return vs.back();
  }
  std::size_t i = 0;
  while (us[i + 1] < u) {
    ++i;
  }
  const auto slope = [&](std::size_t j) {
    const std::size_t lo = j > 0 ? j - 1 : 0;
    const std::size_t hi = std::min(j + 1, N - 1);
    return (vs[hi] - vs[lo]) / (us[hi] - us[lo]);
  };
  const float h = us[i + 1] - us[i];
  const float t = (u - us[i]) / h;
  const float t2 = t * t;
  const float t3 = t2 * t;
  return (2.0f * t3 - 3.0f * t2 + 1.0f) * vs[i] +
      (t3 - 2.0f * t2 + t) * h * slope(i) +
      (-2.0f * t3 + 3.0f * t2) * vs[i + 1] +
      (t3 - t2) * h * slope(i + 1);
}

class MeshBuilder {
public:
  MeshBuilder(ReefFishMeshData& mesh, float halfWidth, float halfHeight)
      : mesh_(mesh), halfWidth_(halfWidth), halfHeight_(halfHeight) {
  }

  float x(float u) const {
    return halfWidth_ * (1.0f - 2.0f * u);
  }

  float y(float v) const {
    return halfHeight_ * (1.0f - 2.0f * v);
  }

  float v(float yValue) const {
    return 0.5f - yValue / (2.0f * halfHeight_);
  }

  float halfHeight() const {
    return halfHeight_;
  }

  /// Point on the lofted body; theta = 0 is the dorsal ridge, pi/2 is +z.
  Vec3 bodyPoint(float u, float theta) const {
    const ReefFishBodyEdges edges = reefFishBodyEdges(u);
    const float top = y(edges.top);
    const float bottom = y(edges.bottom);
    const float center = 0.5f * (top + bottom);
    const float radius = 0.5f * (top - bottom);
    const float halfThickness = radius * reefFishThicknessRatio(u);
    return {x(u), center + radius * std::cos(theta),
            halfThickness * std::sin(theta)};
  }

  /// Distance of the body surface from the midplane at (u, y).
  float bodySurfaceZ(float u, float yValue) const {
    const ReefFishBodyEdges edges = reefFishBodyEdges(u);
    const float top = y(edges.top);
    const float bottom = y(edges.bottom);
    const float center = 0.5f * (top + bottom);
    const float radius = 0.5f * (top - bottom);
    const float offset = (yValue - center) / std::max(radius, 1.0e-5f);
    return radius * reefFishThicknessRatio(u) *
        std::sqrt(std::max(0.0f, 1.0f - offset * offset));
  }

  Vec3 bodyNormal(float u, float theta) const {
    constexpr float kStepU = 1.0e-3f;
    constexpr float kStepTheta = 1.0e-3f;
    const Vec3 alongU = bodyPoint(u + kStepU, theta) - bodyPoint(u - kStepU, theta);
    const Vec3 alongTheta =
        bodyPoint(u, theta + kStepTheta) - bodyPoint(u, theta - kStepTheta);
    const Vec3 point = bodyPoint(u, theta);
    const ReefFishBodyEdges edges = reefFishBodyEdges(u);
    const Vec3 radial{0.0f, point.y - 0.5f * (y(edges.top) + y(edges.bottom)),
                      point.z};
    return normalize(cross(alongU, alongTheta), normalize(radial, {0, 1, 0}));
  }

  std::uint32_t vertex(const Vec3& position, const Vec3& normal,
                       float u, float vValue, float surfaceType) {
    const std::uint32_t index =
        static_cast<std::uint32_t>(mesh_.vertices.size());
    mesh_.vertices.push_back(ReefFishVertex{
        {position.x, position.y, position.z},
        {normal.x, normal.y, normal.z},
        {std::clamp(u, 0.0f, 1.0f), std::clamp(vValue, 0.0f, 1.0f)},
        surfaceType});
    return index;
  }

  /// Emits a triangle wound counter-clockwise around <paramref name="facing"/>.
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c,
                const Vec3& facing) {
    const Vec3 pa = position(a);
    const Vec3 face = cross(position(b) - pa, position(c) - pa);
    if (dot(face, facing) < 0.0f) {
      std::swap(b, c);
    }
    mesh_.indices.insert(mesh_.indices.end(), {a, b, c});
  }

  /// Fin quad visible from both sides; each side gets its own normal.
  void doubleSidedQuad(const std::array<Vec3, 4>& corners,
                       const std::array<std::array<float, 2>, 4>& uvs) {
    const Vec3 normal = normalize(
        cross(corners[1] - corners[0], corners[3] - corners[0]),
        {0.0f, 0.0f, 1.0f});
    for (const float side : {1.0f, -1.0f}) {
      const Vec3 sideNormal = normal * side;
      std::array<std::uint32_t, 4> ids{};
      for (std::size_t i = 0; i < 4; ++i) {
        ids[i] = vertex(corners[i], sideNormal, uvs[i][0], uvs[i][1],
                        kReefFishFinSurface);
      }
      triangle(ids[0], ids[1], ids[2], sideNormal);
      triangle(ids[0], ids[2], ids[3], sideNormal);
    }
  }

  /// Midplane fin strip between two texture-v curves.
  template <typename Outer, typename Inner>
  void finStrip(float startU, float endU, int steps, Outer outerV, Inner innerV) {
    for (int step = 0; step < steps; ++step) {
      const float u0 = mix(startU, endU, static_cast<float>(step) / steps);
      const float u1 = mix(startU, endU, static_cast<float>(step + 1) / steps);
      const float a0 = outerV(u0);
      const float a1 = outerV(u1);
      const float b0 = innerV(u0);
      const float b1 = innerV(u1);
      doubleSidedQuad(
          {Vec3{x(u0), y(a0), 0.0f}, Vec3{x(u1), y(a1), 0.0f},
           Vec3{x(u1), y(b1), 0.0f}, Vec3{x(u0), y(b0), 0.0f}},
          {{{u0, a0}, {u1, a1}, {u1, b1}, {u0, b0}}});
    }
  }

private:
  Vec3 position(std::uint32_t index) const {
    const ReefFishVertex& value = mesh_.vertices[index];
    return {value.position[0], value.position[1], value.position[2]};
  }

  ReefFishMeshData& mesh_;
  float halfWidth_;
  float halfHeight_;
};

void buildBody(MeshBuilder& builder, ReefFishMeshData& mesh) {
  const auto ringVertex = [](int section, int segment) {
    return static_cast<std::uint32_t>(
        section * kBodyRingSegments + segment % kBodyRingSegments);
  };
  for (int section = 0; section <= kBodySections; ++section) {
    const float u = mix(kReefFishBodyStartU, kReefFishBodyEndU,
                        static_cast<float>(section) / kBodySections);
    for (int segment = 0; segment < kBodyRingSegments; ++segment) {
      const float theta = kTwoPi * static_cast<float>(segment) / kBodyRingSegments;
      const Vec3 point = builder.bodyPoint(u, theta);
      builder.vertex(point, builder.bodyNormal(u, theta), u,
                     builder.v(point.y), kReefFishBodySurface);
    }
  }
  const auto facing = [&mesh](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    Vec3 sum{0.0f, 0.0f, 0.0f};
    for (const std::uint32_t id : {a, b, c}) {
      const float* n = mesh.vertices[id].normal;
      sum = sum + Vec3{n[0], n[1], n[2]};
    }
    return sum;
  };
  for (int section = 0; section < kBodySections; ++section) {
    for (int segment = 0; segment < kBodyRingSegments; ++segment) {
      const std::uint32_t a = ringVertex(section, segment);
      const std::uint32_t b = ringVertex(section, segment + 1);
      const std::uint32_t c = ringVertex(section + 1, segment + 1);
      const std::uint32_t d = ringVertex(section + 1, segment);
      builder.triangle(a, b, c, facing(a, b, c));
      builder.triangle(a, c, d, facing(a, c, d));
    }
  }

  // Snout and peduncle caps close the loft so the body stays watertight.
  const auto cap = [&](int section, float capU, float direction) {
    const float u = mix(kReefFishBodyStartU, kReefFishBodyEndU,
                        static_cast<float>(section) / kBodySections);
    const ReefFishBodyEdges edges = reefFishBodyEdges(u);
    const float centerY =
        0.5f * (builder.y(edges.top) + builder.y(edges.bottom));
    const Vec3 axis{direction, 0.0f, 0.0f};
    const std::uint32_t center = builder.vertex(
        {builder.x(capU), centerY, 0.0f}, axis, capU,
        builder.v(centerY), kReefFishBodySurface);
    for (int segment = 0; segment < kBodyRingSegments; ++segment) {
      builder.triangle(center, ringVertex(section, segment),
                       ringVertex(section, segment + 1), axis);
    }
  };
  cap(0, kReefFishBodyStartU - 0.006f, 1.0f);
  cap(kBodySections, kReefFishBodyEndU, -1.0f);
}

void buildFins(MeshBuilder& builder) {
  builder.finStrip(0.240f, kReefFishBodyEndU, kFinStripSteps,
      [](float u) { return std::max(0.0f, reefFishDorsalEdge(u) - 0.015f); },
      [](float u) { return reefFishBodyEdges(u).top + kFinOverlap; });
  builder.finStrip(0.240f, kReefFishBodyEndU, kFinStripSteps,
      [](float u) {
        return std::min(1.0f, u < 0.46f ? kPelvicLowestV + 0.015f
                                        : reefFishVentralEdge(u) + 0.015f);
      },
      [](float u) { return reefFishBodyEdges(u).bottom - kFinOverlap; });
  builder.finStrip(0.800f, kReefFishTailEndU + 0.005f, kTailSteps,
      [](float) { return 0.225f; }, [](float) { return 0.775f; });

  // Pectoral fins: flared quads on both flanks sampling the corner atlas.
  const float atlasU0 = kReefFishPectoralAtlas[0];
  const float atlasU1 = kReefFishPectoralAtlas[1];
  const float atlasV0 = kReefFishPectoralAtlas[2];
  const float atlasV1 = kReefFishPectoralAtlas[3];
  constexpr int kPectoralSteps = 3;
  constexpr float kBaseU = 0.265f;
  constexpr float kTipU = 0.400f;
  for (const float side : {1.0f, -1.0f}) {
    for (int step = 0; step < kPectoralSteps; ++step) {
      const float s0 = static_cast<float>(step) / kPectoralSteps;
      const float s1 = static_cast<float>(step + 1) / kPectoralSteps;
      const auto edgePoint = [&](float s, bool lower) {
        const float u = mix(kBaseU, kTipU, s);
        const float vValue = lower ? mix(0.625f, 0.675f, s)
                                   : mix(0.555f, 0.585f, s);
        const float yValue = builder.y(vValue);
        const float flare = 0.004f + 0.10f * builder.halfHeight() *
            std::pow(s, 1.3f);
        return Vec3{builder.x(u), yValue,
                    side * (builder.bodySurfaceZ(u, yValue) + flare)};
      };
      builder.doubleSidedQuad(
          {edgePoint(s0, false), edgePoint(s1, false),
           edgePoint(s1, true), edgePoint(s0, true)},
          {{{mix(atlasU0, atlasU1, s0), atlasV0},
            {mix(atlasU0, atlasU1, s1), atlasV0},
            {mix(atlasU0, atlasU1, s1), atlasV1},
            {mix(atlasU0, atlasU1, s0), atlasV1}}});
    }
  }
}

}  // namespace

ReefFishBodyEdges reefFishBodyEdges(float u) {
  return {hermite(kBodyU, kBodyTop, u), hermite(kBodyU, kBodyBottom, u)};
}

float reefFishDorsalEdge(float u) {
  return hermite(kDorsalU, kDorsalV, u);
}

float reefFishVentralEdge(float u) {
  return hermite(kVentralU, kVentralV, u);
}

float reefFishThicknessRatio(float u) {
  // Deep, laterally compressed disc with a rounder snout and peduncle.
  const float body = mix(0.42f, 0.24f, smoothstep(0.05f, 0.35f, u));
  return mix(body, 0.55f, smoothstep(0.70f, 0.84f, u));
}

ReefFishMeshData buildReefFishMesh(float halfWidth, float halfHeight) {
  ReefFishMeshData mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(
      (kBodySections + 1) * kBodyRingSegments + 2 +
      (2 * kFinStripSteps + kTailSteps + 6) * 8));
  MeshBuilder builder(mesh, halfWidth, halfHeight);
  buildBody(builder, mesh);
  buildFins(builder);
  return mesh;
}

}  // namespace hg::render::gl33
