#include "render/gl33/World11TubeSpongeGeometry.hpp"

#include "world/World11Landmarks.hpp"
#include "world/World11Seabed.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
constexpr std::uint64_t kColonySeed = 0x53504F4E47453131ULL;
constexpr int kRingSegments = 28;
constexpr int kRimSteps = 6;
constexpr float kRingSpacing = 0.07f;
// The wall starts below t = 0 so the flared foot is buried in the sand.
constexpr float kBuriedT = -0.12f;
// Outward bend towards vertical along the tube.
constexpr float kUpwardCurl = 0.35f;

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 operator+(const Vec3& a, const Vec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& value, float factor) {
  return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalize(const Vec3& value, const Vec3& fallback) {
  const float length = std::sqrt(dot(value, value));
  return length < 1.0e-8f ? fallback : value * (1.0f / length);
}

Vec3 toVec(const std::array<float, 3>& value) {
  return {value[0], value[1], value[2]};
}

float smoothstep(float edge0, float edge1, float value) {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

std::uint64_t mixHash(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

float unitFrom(std::uint64_t& state) {
  state = mixHash(state);
  return static_cast<float>(state >> 40U) * (1.0f / 16777216.0f);
}

const Vec3 kUp{0.0f, 1.0f, 0.0f};

/// Axis frame of a tube: centre line, tangent and two unit radials.
struct TubeFrame {
  Vec3 center;
  Vec3 tangent;
  Vec3 side;
  Vec3 across;
};

Vec3 curlDirection(const Vec3& direction) {
  return kUp - direction * dot(kUp, direction);
}

TubeFrame frameAt(const TubeSpongeTube& tube, float t) {
  const Vec3 direction = toVec(tube.direction);
  const Vec3 curl = curlDirection(direction);
  const Vec3 center = toVec(tube.base) +
      (direction * t + curl * (kUpwardCurl * t * t)) * tube.height;
  const Vec3 tangent = normalize(
      direction + curl * (2.0f * kUpwardCurl * t), direction);
  // The axis stays in the plane of up and the growth direction, so the
  // horizontal side vector is perpendicular to it everywhere.
  const Vec3 side = normalize(cross(kUp, direction), {1.0f, 0.0f, 0.0f});
  const Vec3 across = normalize(cross(tangent, side), {0.0f, 0.0f, 1.0f});
  return {center, tangent, side, across};
}

Vec3 radial(const TubeFrame& frame, float theta) {
  return frame.side * std::cos(theta) + frame.across * std::sin(theta);
}

float outerRadius(const TubeSpongeTube& tube, float t) {
  const float phase = static_cast<float>(tube.seed % 1024U) * 0.013f;
  const float flare = 1.0f + 0.28f * (1.0f - smoothstep(0.0f, 0.18f, t));
  const float wobble = 1.0f + 0.04f * std::sin(t * tube.height * 2.1f + phase);
  return tube.radius * (0.94f + 0.10f * std::max(t, 0.0f)) * flare * wobble;
}

/// Knobbly relief of the outer skin; fades out at the rim so it meets the lip.
float knobs(const TubeSpongeTube& tube, float t, float theta) {
  const float p = static_cast<float>(tube.seed % 4096U) * 0.0021f;
  const float along = t * tube.height;
  const float relief =
      0.55f * std::sin(theta * 7.0f + p * 3.0f + along * 1.3f) *
          std::sin(along * 6.0f + p * 5.0f) +
      0.45f * std::sin(theta * 12.0f + p * 7.0f - along * 2.2f) *
          std::sin(along * 9.5f + p * 11.0f);
  return tube.radius * 0.12f * relief * (1.0f - smoothstep(0.88f, 0.99f, t));
}

Vec3 outerPoint(const TubeSpongeTube& tube, float t, float theta) {
  const TubeFrame frame = frameAt(tube, t);
  return frame.center +
      radial(frame, theta) * (outerRadius(tube, t) + knobs(tube, t, theta));
}

Vec3 innerPoint(const TubeSpongeTube& tube, float t, float theta) {
  const TubeFrame frame = frameAt(tube, t);
  return frame.center +
      radial(frame, theta) * (outerRadius(tube, t) - tube.wallThickness);
}

class Builder {
public:
  explicit Builder(TubeSpongeMeshData& mesh) : mesh_(mesh) {
  }

  std::uint32_t vertex(const Vec3& position, const Vec3& normal,
                       float u, float v) {
    const auto index = static_cast<std::uint32_t>(mesh_.vertices.size());
    mesh_.vertices.push_back(TubeSpongeVertex{
        {position.x, position.y, position.z},
        {normal.x, normal.y, normal.z}, {u, v}, 0.0f});
    return index;
  }

  /// Emits a triangle wound counter-clockwise around its vertex normals.
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const Vec3 pa = position(a);
    const Vec3 face = cross(position(b) - pa, position(c) - pa);
    if (dot(face, normal(a) + normal(b) + normal(c)) < 0.0f) {
      std::swap(b, c);
    }
    mesh_.indices.insert(mesh_.indices.end(), {a, b, c});
  }

  /// Quads between consecutive rings of (segments + 1) vertices each.
  void connectRings(std::uint32_t firstRing, int ringCount) {
    constexpr std::uint32_t stride = kRingSegments + 1;
    for (int ring = 0; ring + 1 < ringCount; ++ring) {
      for (std::uint32_t segment = 0; segment < kRingSegments; ++segment) {
        const std::uint32_t a = firstRing + ring * stride + segment;
        const std::uint32_t b = a + 1;
        const std::uint32_t c = a + stride + 1;
        const std::uint32_t d = a + stride;
        triangle(a, b, c);
        triangle(a, c, d);
      }
    }
  }

  std::uint32_t size() const {
    return static_cast<std::uint32_t>(mesh_.vertices.size());
  }

private:
  Vec3 position(std::uint32_t index) const {
    const float* p = mesh_.vertices[index].position;
    return {p[0], p[1], p[2]};
  }

  Vec3 normal(std::uint32_t index) const {
    const float* n = mesh_.vertices[index].normal;
    return {n[0], n[1], n[2]};
  }

  TubeSpongeMeshData& mesh_;
};

/// Surface normal by central differences, oriented along <paramref name="outward"/>.
template <typename Surface>
Vec3 surfaceNormal(const Surface& surface, float t, float theta,
                   const Vec3& outward) {
  constexpr float kStep = 1.0e-3f;
  const Vec3 alongT = surface(t + kStep, theta) - surface(t - kStep, theta);
  const Vec3 alongTheta = surface(t, theta + kStep) - surface(t, theta - kStep);
  Vec3 normal = normalize(cross(alongT, alongTheta), outward);
  if (dot(normal, outward) < 0.0f) {
    normal = normal * -1.0f;
  }
  return normal;
}

void buildTube(const TubeSpongeTube& tube, TubeSpongeMeshData& outerMesh,
               TubeSpongeMeshData& innerMesh) {
  const float aroundRepeats = std::max(
      2.0f, std::round(kTwoPi * tube.radius / kTubeSpongeTextureTile));
  const float lengthToV = tube.height / kTubeSpongeTextureTile;
  const auto uAt = [&](int segment) {
    return aroundRepeats * static_cast<float>(segment) / kRingSegments;
  };
  const auto thetaAt = [](int segment) {
    return kTwoPi * static_cast<float>(segment) / kRingSegments;
  };
  const auto outerSurface = [&tube](float t, float theta) {
    return outerPoint(tube, t, theta);
  };
  const auto innerSurface = [&tube](float t, float theta) {
    return innerPoint(tube, t, theta);
  };

  // Outer wall, from the buried foot to the rim.
  Builder outer(outerMesh);
  const int outerRings = std::max(
      8, static_cast<int>(std::ceil((1.0f - kBuriedT) * tube.height / kRingSpacing)));
  const std::uint32_t outerStart = outer.size();
  for (int ring = 0; ring <= outerRings; ++ring) {
    const float t = kBuriedT + (1.0f - kBuriedT) * ring / outerRings;
    const TubeFrame frame = frameAt(tube, t);
    for (int segment = 0; segment <= kRingSegments; ++segment) {
      const float theta = thetaAt(segment);
      outer.vertex(outerPoint(tube, t, theta),
                   surfaceNormal(outerSurface, t, theta, radial(frame, theta)),
                   uAt(segment), t * lengthToV);
    }
  }
  outer.connectRings(outerStart, outerRings + 1);

  // Rounded lip joining the outer wall to the cavity wall.
  const TubeFrame top = frameAt(tube, 1.0f);
  const float lipRadius = 0.5f * tube.wallThickness;
  const float lipCenter = outerRadius(tube, 1.0f) - lipRadius;
  const std::uint32_t rimStart = outer.size();
  for (int step = 0; step <= kRimSteps; ++step) {
    const float alpha = kPi * static_cast<float>(step) / kRimSteps;
    for (int segment = 0; segment <= kRingSegments; ++segment) {
      const Vec3 direction = radial(top, thetaAt(segment));
      const Vec3 position = top.center +
          direction * (lipCenter + lipRadius * std::cos(alpha)) +
          top.tangent * (lipRadius * 0.9f * std::sin(alpha));
      const Vec3 normal = normalize(
          direction * std::cos(alpha) + top.tangent * std::sin(alpha),
          top.tangent);
      outer.vertex(position, normal, uAt(segment),
                   lengthToV + alpha * lipRadius / kTubeSpongeTextureTile);
    }
  }
  outer.connectRings(rimStart, kRimSteps + 1);

  // Cavity wall, from the rim down to the floor; normals face the axis.
  Builder inner(innerMesh);
  const float cavityTop = lengthToV + kPi * lipRadius / kTubeSpongeTextureTile;
  const int innerRings = std::max(
      4, static_cast<int>(std::ceil(
          (1.0f - kTubeSpongeCavityFloor) * tube.height / kRingSpacing)));
  const std::uint32_t innerStart = inner.size();
  for (int ring = 0; ring <= innerRings; ++ring) {
    const float t = 1.0f - (1.0f - kTubeSpongeCavityFloor) * ring / innerRings;
    const TubeFrame frame = frameAt(tube, t);
    for (int segment = 0; segment <= kRingSegments; ++segment) {
      const float theta = thetaAt(segment);
      inner.vertex(innerPoint(tube, t, theta),
                   surfaceNormal(innerSurface, t, theta,
                                 radial(frame, theta) * -1.0f),
                   uAt(segment), cavityTop + (1.0f - t) * lengthToV);
    }
  }
  inner.connectRings(innerStart, innerRings + 1);

  // Cavity floor.
  const TubeFrame floor = frameAt(tube, kTubeSpongeCavityFloor);
  const std::uint32_t floorCenter = inner.vertex(
      floor.center, floor.tangent, 0.5f * aroundRepeats, 0.0f);
  const std::uint32_t floorRing = inner.size();
  const float floorRadius =
      outerRadius(tube, kTubeSpongeCavityFloor) - tube.wallThickness;
  for (int segment = 0; segment <= kRingSegments; ++segment) {
    const Vec3 direction = radial(floor, thetaAt(segment));
    inner.vertex(floor.center + direction * floorRadius, floor.tangent,
                 0.5f * aroundRepeats + direction.x * floorRadius /
                     kTubeSpongeTextureTile,
                 direction.z * floorRadius / kTubeSpongeTextureTile);
  }
  for (std::uint32_t segment = 0; segment < kRingSegments; ++segment) {
    inner.triangle(floorCenter, floorRing + segment, floorRing + segment + 1);
  }
}

struct ClusterSpec {
  float offsetX;
  float offsetZ;
  int tubeCount;
  float tallest;
};

// Four clumps around the landmark; the central one is the tallest.
constexpr std::array<ClusterSpec, 4> kClusters = {{
    {0.0f, 0.0f, 5, 3.2f},
    {2.7f, 1.5f, 3, 2.2f},
    {-2.4f, 1.9f, 4, 2.6f},
    {0.9f, -2.8f, 3, 1.9f},
}};

}  // namespace

std::vector<TubeSpongeTube> world11TubeSpongeColony() {
  const world::World11Landmark colony = world::kWorld11Colony;
  std::vector<TubeSpongeTube> tubes;
  for (std::size_t cluster = 0; cluster < kClusters.size(); ++cluster) {
    const ClusterSpec& spec = kClusters[cluster];
    for (int index = 0; index < spec.tubeCount; ++index) {
      std::uint64_t state = mixHash(kColonySeed ^ (cluster * 64U + index));
      const bool leader = index == 0;
      const float azimuth = static_cast<float>(index) * 2.39996f +
          static_cast<float>(cluster) * 0.9f + 0.4f * unitFrom(state);
      const float tilt = leader ? 0.08f + 0.08f * unitFrom(state)
                                : 0.24f + 0.26f * unitFrom(state);
      const float height = spec.tallest *
          (leader ? 1.0f : 0.55f + 0.35f * unitFrom(state));
      const float radius = leader ? 0.36f : 0.25f + 0.09f * unitFrom(state);
      const float footOffset = leader ? 0.0f : 0.24f + 0.10f * unitFrom(state);

      const float x = colony.x + spec.offsetX + std::cos(azimuth) * footOffset;
      const float z = colony.z + spec.offsetZ + std::sin(azimuth) * footOffset;
      TubeSpongeTube tube;
      tube.base = {x, world::world11SeabedHeight(x, z) - 0.20f, z};
      tube.direction = {std::sin(tilt) * std::cos(azimuth), std::cos(tilt),
                        std::sin(tilt) * std::sin(azimuth)};
      tube.height = height;
      tube.radius = radius;
      tube.wallThickness = radius * 0.28f;
      tube.seed = static_cast<std::uint32_t>(mixHash(state) >> 32U);
      tubes.push_back(tube);
    }
  }
  return tubes;
}

std::array<float, 3> tubeSpongeCenter(const TubeSpongeTube& tube, float t) {
  const Vec3 center = frameAt(tube, t).center;
  return {center.x, center.y, center.z};
}

TubeSpongeColonyMesh buildTubeSpongeColonyMesh(
    const std::vector<TubeSpongeTube>& tubes) {
  TubeSpongeColonyMesh mesh;
  for (const TubeSpongeTube& tube : tubes) {
    buildTube(tube, mesh.outer, mesh.inner);
  }
  return mesh;
}

}  // namespace hg::render::gl33
