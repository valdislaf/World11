#include "render/gl33/World11CoralGeometry.hpp"

#include "world/World11DecorGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

namespace hg::render::gl33 {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kAreaExponent = 2.4f;
constexpr float kGoldenAngle = 2.39996322972865332f;

CoralVec3 add(const CoralVec3& a, const CoralVec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

CoralVec3 subtract(const CoralVec3& a, const CoralVec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

CoralVec3 multiply(const CoralVec3& value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

float dot(const CoralVec3& a, const CoralVec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

CoralVec3 cross(const CoralVec3& a, const CoralVec3& b) {
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

float length(const CoralVec3& value) {
  return std::sqrt(dot(value, value));
}

CoralVec3 normalize(const CoralVec3& value) {
  const float magnitude = length(value);
  if (magnitude < 1.0e-7f) {
    return {0.0f, 1.0f, 0.0f};
  }
  return multiply(value, 1.0f / magnitude);
}

float clamp01(float value) {
  return std::max(0.0f, std::min(value, 1.0f));
}

float radians(float degrees) {
  return degrees * kPi / 180.0f;
}

CoralVec3 rotateAroundAxis(const CoralVec3& value,
                           const CoralVec3& axisValue, float angle) {
  const CoralVec3 axis = normalize(axisValue);
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  return add(add(multiply(value, cosine),
                 multiply(cross(axis, value), sine)),
             multiply(axis, dot(axis, value) * (1.0f - cosine)));
}

CoralVec3 perpendicular(const CoralVec3& direction) {
  const CoralVec3 reference = std::abs(direction.y) < 0.92f
      ? CoralVec3{0.0f, 1.0f, 0.0f}
      : CoralVec3{1.0f, 0.0f, 0.0f};
  return normalize(cross(direction, reference));
}

class CoralRandom final {
public:
  explicit CoralRandom(std::uint64_t seed) : state_(seed) {}

  std::uint64_t next64() {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state_;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
  }

  float unit() {
    return static_cast<float>(next64() >> 40U) * (1.0f / 16777216.0f);
  }

  float range(float minimum, float maximum) {
    return minimum + (maximum - minimum) * unit();
  }

private:
  std::uint64_t state_;
};

std::vector<float> areaPreservingRadii(float parentRadius,
                                       const std::vector<float>& weights,
                                       float areaBudget) {
  float poweredWeightSum = 0.0f;
  for (const float weight : weights) {
    poweredWeightSum += std::pow(weight, kAreaExponent);
  }
  const float normalization = std::pow(
      areaBudget / std::max(poweredWeightSum, 1.0e-8f),
      1.0f / kAreaExponent);
  std::vector<float> radii;
  radii.reserve(weights.size());
  for (const float weight : weights) {
    radii.push_back(parentRadius * weight * normalization);
  }
  return radii;
}

CoralVec3 directionAtInclination(const CoralVec3& parentDirection,
                                 float inclination, float azimuth) {
  const CoralVec3 parent = normalize(parentDirection);
  const CoralVec3 sideA = perpendicular(parent);
  const CoralVec3 sideB = normalize(cross(parent, sideA));
  const CoralVec3 radial = add(multiply(sideA, std::cos(azimuth)),
                               multiply(sideB, std::sin(azimuth)));
  return normalize(add(multiply(parent, std::cos(inclination)),
                       multiply(radial, std::sin(inclination))));
}

class SkeletonBuildContext final {
public:
  SkeletonBuildContext(CoralVariantType type, std::uint64_t seed)
      : type_(type), random_(seed) {
    skeleton_.type = type;
    skeleton_.variantSeed = seed;
  }

  CoralSkeleton build() {
    CoralSkeletonBranch root;
    root.start = {0.0f, 0.0f, 0.0f};
    root.end = {0.0f, random_.range(0.10f, 0.14f), 0.0f};
    root.jointCenter = root.start;
    root.startTangent = {0.0f, 1.0f, 0.0f};
    root.endTangent = {0.0f, 1.0f, 0.0f};
    root.startRadius = random_.range(0.09f, 0.14f);
    root.endRadius = root.startRadius * random_.range(0.86f, 0.92f);
    root.branchTint = random_.range(-0.055f, 0.055f);
    root.tipBulbScale = random_.range(1.02f, 1.12f);
    root.level = 0;
    root.terminal = false;
    root.significance = 1000.0f;
    skeleton_.branches.push_back(root);

    addMainCrown();
    normalizeSkeleton();
    validateAreaGroups();
    return skeleton_;
  }

private:
  void addMainCrown() {
    std::uint32_t count = 5U;
    if (type_ == CoralVariantType::Tree) {
      count = 4U + static_cast<std::uint32_t>(random_.unit() > 0.62f);
    } else if (type_ == CoralVariantType::Bushy) {
      count = 5U + static_cast<std::uint32_t>(random_.unit() > 0.48f);
    } else if (type_ == CoralVariantType::LowSpreading) {
      count = 4U + static_cast<std::uint32_t>(random_.unit() > 0.35f);
    }

    std::vector<float> weights(count, 1.0f);
    for (std::uint32_t index = 0; index < count; ++index) {
      weights[index] = random_.range(0.90f, 1.10f);
      if (type_ == CoralVariantType::Tree && index == 0U) {
        weights[index] = 1.18f;
      }
    }
    const float budget = random_.range(0.84f, 0.93f);
    const std::vector<float> radii = areaPreservingRadii(
        skeleton_.branches[0].endRadius, weights, budget);
    CoralAreaGroup group;
    group.parentBranch = 0;
    group.areaBudget = budget;
    const float baseAzimuth = random_.range(0.0f, kTwoPi);

    for (std::uint32_t index = 0; index < count; ++index) {
      const CoralVec3 direction = mainDirection(index, count, baseAzimuth);
      const float branchLength = mainLength(index);
      const std::uint32_t child = appendChild(
          0U, direction, branchLength, radii[index], 1U,
          static_cast<std::uint32_t>(skeleton_.areaGroups.size()));
      group.childBranches.push_back(child);
    }
    skeleton_.areaGroups.push_back(group);

    const std::vector<std::uint32_t> mainBranches = group.childBranches;
    for (const std::uint32_t branch : mainBranches) {
      grow(branch);
    }
  }

  CoralVec3 mainDirection(std::uint32_t index, std::uint32_t count,
                          float baseAzimuth) {
    if (type_ == CoralVariantType::Fan) {
      const float unit = count <= 1U ? 0.0f :
          static_cast<float>(index) / static_cast<float>(count - 1U);
      const float signedAngle = radians(-54.0f + 108.0f * unit) +
          random_.range(radians(-4.0f), radians(4.0f));
      return normalize({
          std::sin(signedAngle),
          std::max(0.22f, std::cos(signedAngle)),
          random_.range(-std::tan(radians(12.0f)),
                        std::tan(radians(12.0f))) * 0.35f,
      });
    }

    float inclination = random_.range(radians(30.0f), radians(49.0f));
    float azimuth = baseAzimuth + kTwoPi * static_cast<float>(index) /
        static_cast<float>(count) + random_.range(radians(-7.0f), radians(7.0f));
    if (type_ == CoralVariantType::Tree) {
      inclination = index == 0U
          ? random_.range(radians(7.0f), radians(15.0f))
          : random_.range(radians(38.0f), radians(52.0f));
      azimuth = baseAzimuth + kGoldenAngle * static_cast<float>(index);
    } else if (type_ == CoralVariantType::LowSpreading) {
      inclination = random_.range(radians(54.0f), radians(68.0f));
    }
    return directionAtInclination({0.0f, 1.0f, 0.0f}, inclination, azimuth);
  }

  float mainLength(std::uint32_t index) {
    if (type_ == CoralVariantType::Bushy) {
      return random_.range(0.52f, 0.68f);
    }
    if (type_ == CoralVariantType::Fan) {
      return random_.range(0.58f, 0.76f);
    }
    if (type_ == CoralVariantType::Tree) {
      return index == 0U ? random_.range(0.64f, 0.74f)
                         : random_.range(0.54f, 0.67f);
    }
    return random_.range(0.50f, 0.64f);
  }

  std::uint32_t appendChild(std::uint32_t parentIndex,
                            CoralVec3 direction, float branchLength,
                            float startRadius, std::uint32_t level,
                            std::uint32_t groupIndex) {
    CoralSkeletonBranch& parent = skeleton_.branches[parentIndex];
    direction.y = std::max(direction.y, type_ == CoralVariantType::LowSpreading
        ? 0.16f : 0.22f);
    direction = normalize(direction);
    const float embedDepth = random_.range(0.20f, 0.35f) * parent.endRadius;
    CoralSkeletonBranch branch;
    branch.parent = parentIndex;
    branch.level = level;
    branch.outgoingGroup = groupIndex;
    branch.jointCenter = parent.end;
    branch.start = subtract(parent.end,
                            multiply(parent.endTangent, embedDepth));
    branch.end = add(branch.start, multiply(direction, branchLength));
    branch.startTangent = normalize(add(
        multiply(parent.endTangent, level == 1U ? 0.15f : 0.28f),
        multiply(direction, level == 1U ? 0.85f : 0.72f)));
    const CoralVec3 curveAxis = perpendicular(direction);
    branch.endTangent = normalize(add(
        direction,
        multiply(rotateAroundAxis(curveAxis, direction,
                                  random_.range(0.0f, kTwoPi)),
                 random_.range(-0.30f, 0.30f))));
    branch.endTangent.y = std::max(branch.endTangent.y, 0.12f);
    branch.endTangent = normalize(branch.endTangent);
    branch.startRadius = startRadius;
    branch.endRadius = startRadius * random_.range(0.80f, 0.90f);
    branch.branchTint = random_.range(-0.075f, 0.075f);
    branch.tipBulbScale = random_.range(1.0f, 1.20f);
    branch.significance = std::pow(startRadius, kAreaExponent) * branchLength;
    branch.terminal = true;
    skeleton_.branches.push_back(branch);
    return static_cast<std::uint32_t>(skeleton_.branches.size() - 1U);
  }

  void grow(std::uint32_t branchIndex) {
    const std::uint32_t maximumLevel = type_ == CoralVariantType::Tree ? 4U : 3U;
    if (skeleton_.branches[branchIndex].level >= maximumLevel ||
        skeleton_.branches.size() + 2U > 38U) {
      return;
    }

    const CoralSkeletonBranch parentSnapshot = skeleton_.branches[branchIndex];
    const float parentLength = length(subtract(
        parentSnapshot.end, parentSnapshot.start));
    if (parentLength < 0.13f || parentSnapshot.endRadius < 0.014f) {
      return;
    }

    constexpr std::uint32_t childCount = 2U;
    std::vector<float> weights{
        random_.range(1.05f, 1.18f), random_.range(0.88f, 1.04f)};
    const float budget = random_.range(0.82f, 0.94f);
    const std::vector<float> radii = areaPreservingRadii(
        parentSnapshot.endRadius, weights, budget);
    CoralAreaGroup group;
    group.parentBranch = branchIndex;
    group.areaBudget = budget;
    const std::uint32_t groupIndex =
        static_cast<std::uint32_t>(skeleton_.areaGroups.size());
    const float baseAzimuth = nodeAzimuth(branchIndex);

    for (std::uint32_t childIndex = 0; childIndex < childCount; ++childIndex) {
      const bool continuation = childIndex == 0U;
      const float inclination = continuation
          ? random_.range(radians(4.0f), radians(16.0f))
          : random_.range(radians(type_ == CoralVariantType::LowSpreading
                                      ? 45.0f : 32.0f),
                          radians(type_ == CoralVariantType::LowSpreading
                                      ? 60.0f : 55.0f));
      float azimuth = baseAzimuth;
      if (!continuation) {
        azimuth += random_.range(radians(105.0f), radians(245.0f));
      }
      CoralVec3 direction = directionAtInclination(
          parentSnapshot.endTangent, inclination, azimuth);
      if (type_ == CoralVariantType::Fan) {
        direction.z = std::max(-0.25f, std::min(direction.z, 0.25f));
        direction = normalize(direction);
      }
      if (type_ == CoralVariantType::LowSpreading && !continuation) {
        direction.y = std::max(direction.y, 0.16f);
        direction = normalize(direction);
      }
      const float childLength = parentLength * random_.range(
          continuation ? 0.58f : 0.55f,
          continuation ? 0.72f : 0.68f);
      const std::uint32_t child = appendChild(
          branchIndex, direction, childLength, radii[childIndex],
          parentSnapshot.level + 1U, groupIndex);
      group.childBranches.push_back(child);
    }

    skeleton_.branches[branchIndex].terminal = false;
    skeleton_.areaGroups.push_back(group);
    const std::vector<std::uint32_t> children = group.childBranches;
    for (const std::uint32_t child : children) {
      grow(child);
    }
  }

  float nodeAzimuth(std::uint32_t branchIndex) {
    if (type_ == CoralVariantType::Tree) {
      return kGoldenAngle * static_cast<float>(branchIndex) +
          random_.range(radians(-6.0f), radians(6.0f));
    }
    if (type_ == CoralVariantType::Fan) {
      return random_.unit() > 0.5f ? 0.0f : kPi;
    }
    return random_.range(0.0f, kTwoPi);
  }

  void normalizeSkeleton() {
    float maximumY = 0.0f;
    float minimumX = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float minimumZ = std::numeric_limits<float>::max();
    float maximumZ = std::numeric_limits<float>::lowest();
    for (const CoralSkeletonBranch& branch : skeleton_.branches) {
      maximumY = std::max(maximumY, std::max(branch.start.y, branch.end.y));
      minimumX = std::min(minimumX, std::min(branch.start.x, branch.end.x));
      maximumX = std::max(maximumX, std::max(branch.start.x, branch.end.x));
      minimumZ = std::min(minimumZ, std::min(branch.start.z, branch.end.z));
      maximumZ = std::max(maximumZ, std::max(branch.start.z, branch.end.z));
    }
    const float inverseHeight = 1.0f / std::max(maximumY, 0.2f);
    for (CoralSkeletonBranch& branch : skeleton_.branches) {
      branch.start = multiply(branch.start, inverseHeight);
      branch.end = multiply(branch.end, inverseHeight);
      branch.jointCenter = multiply(branch.jointCenter, inverseHeight);
      branch.startRadius *= inverseHeight;
      branch.endRadius *= inverseHeight;
    }

    const float desiredRootRadius = random_.range(0.09f, 0.14f);
    const float radiusScale = desiredRootRadius /
        std::max(skeleton_.branches[0].startRadius, 1.0e-5f);
    for (CoralSkeletonBranch& branch : skeleton_.branches) {
      branch.startRadius *= radiusScale;
      branch.endRadius *= radiusScale;
    }
    skeleton_.rootRadius = desiredRootRadius;

    const float rawWidth = std::max(maximumX - minimumX, maximumZ - minimumZ) *
        inverseHeight;
    float desiredWidth = 1.05f;
    if (type_ == CoralVariantType::Fan) {
      desiredWidth = 1.30f;
    } else if (type_ == CoralVariantType::Tree) {
      desiredWidth = 0.98f;
    } else if (type_ == CoralVariantType::LowSpreading) {
      desiredWidth = 1.30f;
    }
    const float horizontalScale = std::max(
        0.45f, std::min(desiredWidth / std::max(rawWidth, 0.1f), 1.8f));
    for (CoralSkeletonBranch& branch : skeleton_.branches) {
      branch.start.x *= horizontalScale;
      branch.start.z *= horizontalScale;
      branch.end.x *= horizontalScale;
      branch.end.z *= horizontalScale;
      branch.jointCenter.x *= horizontalScale;
      branch.jointCenter.z *= horizontalScale;
      branch.startTangent.x *= horizontalScale;
      branch.startTangent.z *= horizontalScale;
      branch.startTangent = normalize(branch.startTangent);
      branch.endTangent.x *= horizontalScale;
      branch.endTangent.z *= horizontalScale;
      branch.endTangent = normalize(branch.endTangent);
      const float branchLength = length(subtract(branch.end, branch.start));
      const float lateralReach = std::sqrt(
          branch.end.x * branch.end.x + branch.end.z * branch.end.z);
      branch.significance = std::pow(branch.startRadius, kAreaExponent) *
          branchLength * (1.0f + 0.18f * lateralReach);
    }
    skeleton_.branches[0].significance = 1000.0f;
  }

  void validateAreaGroups() const {
    for (const CoralAreaGroup& group : skeleton_.areaGroups) {
      const float parentRadius =
          skeleton_.branches[group.parentBranch].endRadius;
      float childArea = 0.0f;
      for (const std::uint32_t child : group.childBranches) {
        childArea += std::pow(
            skeleton_.branches[child].startRadius, kAreaExponent);
      }
      const float parentArea = std::pow(parentRadius, kAreaExponent);
      assert(childArea <= 0.95f * parentArea + 1.0e-6f);
      assert(std::abs(childArea - group.areaBudget * parentArea) < 1.0e-5f);
    }
  }

  CoralVariantType type_;
  CoralRandom random_;
  CoralSkeleton skeleton_;
};

CoralVec3 hermitePosition(const CoralSkeletonBranch& branch, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
  const float h10 = t3 - 2.0f * t2 + t;
  const float h01 = -2.0f * t3 + 3.0f * t2;
  const float h11 = t3 - t2;
  const float branchLength = length(subtract(branch.end, branch.start));
  const CoralVec3 startVelocity = multiply(
      branch.startTangent, branchLength * 0.78f);
  const CoralVec3 endVelocity = multiply(
      branch.endTangent, branchLength * 0.68f);
  return add(add(multiply(branch.start, h00), multiply(startVelocity, h10)),
             add(multiply(branch.end, h01), multiply(endVelocity, h11)));
}

CoralVec3 hermiteTangent(const CoralSkeletonBranch& branch, float t) {
  const float t2 = t * t;
  const float branchLength = length(subtract(branch.end, branch.start));
  const CoralVec3 startVelocity = multiply(
      branch.startTangent, branchLength * 0.78f);
  const CoralVec3 endVelocity = multiply(
      branch.endTangent, branchLength * 0.68f);
  return normalize(add(
      add(multiply(branch.start, 6.0f * t2 - 6.0f * t),
          multiply(startVelocity, 3.0f * t2 - 4.0f * t + 1.0f)),
      add(multiply(branch.end, -6.0f * t2 + 6.0f * t),
          multiply(endVelocity, 3.0f * t2 - 2.0f * t))));
}

CoralVec3 transportNormal(const CoralVec3& previousTangent,
                          const CoralVec3& tangent,
                          const CoralVec3& previousNormal) {
  const CoralVec3 rotationAxis = cross(previousTangent, tangent);
  const float sine = length(rotationAxis);
  CoralVec3 transported = previousNormal;
  if (sine > 1.0e-6f) {
    const float cosine = std::max(-1.0f, std::min(
        dot(previousTangent, tangent), 1.0f));
    transported = rotateAroundAxis(
        previousNormal, multiply(rotationAxis, 1.0f / sine),
        std::atan2(sine, cosine));
  }
  return normalize(subtract(transported,
                            multiply(tangent, dot(transported, tangent))));
}

CoralVertex makeVertex(const CoralVec3& position, const CoralVec3& normal,
                       const CoralSkeletonBranch& branch) {
  return CoralVertex{
      {position.x, position.y, position.z},
      {normal.x, normal.y, normal.z},
      {clamp01(position.y),
       std::min(static_cast<float>(branch.level) / 4.0f, 1.0f),
       branch.branchTint},
  };
}

std::vector<std::uint32_t> selectBranches(const CoralSkeleton& skeleton,
                                          std::size_t branchLimit,
                                          std::uint32_t maximumLevel) {
  std::vector<bool> selected(skeleton.branches.size(), false);
  selected[0] = true;
  std::size_t selectedCount = 1;
  for (std::size_t index = 1; index < skeleton.branches.size(); ++index) {
    if (skeleton.branches[index].level == 1U && selectedCount < branchLimit) {
      selected[index] = true;
      ++selectedCount;
    }
  }

  while (selectedCount < branchLimit) {
    std::size_t best = skeleton.branches.size();
    float bestScore = -1.0f;
    for (std::size_t index = 1; index < skeleton.branches.size(); ++index) {
      const CoralSkeletonBranch& branch = skeleton.branches[index];
      if (selected[index] || branch.level > maximumLevel ||
          !selected[branch.parent]) {
        continue;
      }
      const float score = branch.significance *
          (1.0f + 0.08f * static_cast<float>(branch.level));
      if (score > bestScore) {
        bestScore = score;
        best = index;
      }
    }
    if (best == skeleton.branches.size()) {
      break;
    }
    selected[best] = true;
    ++selectedCount;
  }

  std::vector<std::uint32_t> result;
  result.reserve(selectedCount);
  for (std::size_t index = 0; index < selected.size(); ++index) {
    if (selected[index]) {
      result.push_back(static_cast<std::uint32_t>(index));
    }
  }
  return result;
}

bool hasSelectedChild(const CoralSkeleton& skeleton,
                      const std::vector<bool>& selected,
                      std::uint32_t parent) {
  for (std::size_t index = 1; index < skeleton.branches.size(); ++index) {
    if (selected[index] && skeleton.branches[index].parent == parent) {
      return true;
    }
  }
  return false;
}

void appendRoundedTip(CoralMeshData& mesh,
                      const CoralSkeletonBranch& branch,
                      std::uint32_t endRing, int ringSides,
                      const CoralVec3& tangent,
                      const CoralVec3& frameNormal,
                      float tipRadius) {
  const CoralVec3 frameBinormal = normalize(cross(tangent, frameNormal));
  std::uint32_t previousRing = endRing;
  const std::array<float, 2> axialOffsets{{0.28f, 0.92f}};
  const std::array<float, 2> radialScales{{
      branch.tipBulbScale, branch.tipBulbScale * 0.62f}};
  for (std::size_t ring = 0; ring < axialOffsets.size(); ++ring) {
    const CoralVec3 center = add(
        branch.end, multiply(tangent, tipRadius * axialOffsets[ring]));
    const std::uint32_t currentRing =
        static_cast<std::uint32_t>(mesh.vertices.size());
    for (int side = 0; side < ringSides; ++side) {
      const float angle = kTwoPi * static_cast<float>(side) /
          static_cast<float>(ringSides);
      const CoralVec3 radial = add(
          multiply(frameNormal, std::cos(angle)),
          multiply(frameBinormal, std::sin(angle)));
      const CoralVec3 normal = normalize(add(
          multiply(radial, ring == 0U ? 0.94f : 0.68f),
          multiply(tangent, ring == 0U ? 0.20f : 0.73f)));
      mesh.vertices.push_back(makeVertex(
          add(center, multiply(radial,
              tipRadius * radialScales[ring])), normal, branch));
    }
    for (int side = 0; side < ringSides; ++side) {
      const int next = (side + 1) % ringSides;
      const std::uint32_t a = previousRing + static_cast<std::uint32_t>(side);
      const std::uint32_t b = previousRing + static_cast<std::uint32_t>(next);
      const std::uint32_t c = currentRing + static_cast<std::uint32_t>(side);
      const std::uint32_t d = currentRing + static_cast<std::uint32_t>(next);
      mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
    }
    previousRing = currentRing;
  }

  const CoralVec3 pole = add(
      branch.end, multiply(tangent, tipRadius * 1.55f));
  const std::uint32_t poleIndex =
      static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(makeVertex(pole, tangent, branch));
  for (int side = 0; side < ringSides; ++side) {
    const int next = (side + 1) % ringSides;
    mesh.indices.insert(mesh.indices.end(), {
        previousRing + static_cast<std::uint32_t>(side),
        poleIndex,
        previousRing + static_cast<std::uint32_t>(next),
    });
  }
}

void appendBranch(CoralMeshData& mesh, const CoralSkeleton& skeleton,
                  const CoralSkeletonBranch& branch, int intervals,
                  int ringSides, bool terminal) {
  const float terminalRadius = std::max({
      0.55f * branch.startRadius, 0.015f,
      0.15f * skeleton.rootRadius});
  const float targetRadius = terminal ? terminalRadius : branch.endRadius;
  CoralVec3 tangent = hermiteTangent(branch, 0.0f);
  CoralVec3 frameNormal = perpendicular(tangent);
  std::uint32_t previousRing = 0;

  for (int interval = 0; interval <= intervals; ++interval) {
    const float t = static_cast<float>(interval) /
        static_cast<float>(intervals);
    const CoralVec3 center = hermitePosition(branch, t);
    const CoralVec3 nextTangent = hermiteTangent(branch, t);
    if (interval != 0) {
      frameNormal = transportNormal(tangent, nextTangent, frameNormal);
    }
    tangent = nextTangent;
    const CoralVec3 frameBinormal = normalize(cross(tangent, frameNormal));
    const float smoothT = t * t * (3.0f - 2.0f * t);
    const float radius = branch.startRadius +
        (targetRadius - branch.startRadius) * smoothT;
    const std::uint32_t ringStart =
        static_cast<std::uint32_t>(mesh.vertices.size());
    for (int side = 0; side < ringSides; ++side) {
      const float angle = kTwoPi * static_cast<float>(side) /
          static_cast<float>(ringSides);
      const CoralVec3 radial = add(
          multiply(frameNormal, std::cos(angle)),
          multiply(frameBinormal, std::sin(angle)));
      mesh.vertices.push_back(makeVertex(
          add(center, multiply(radial, radius)), radial, branch));
    }
    if (interval != 0) {
      for (int side = 0; side < ringSides; ++side) {
        const int next = (side + 1) % ringSides;
        const std::uint32_t a = previousRing +
            static_cast<std::uint32_t>(side);
        const std::uint32_t b = previousRing +
            static_cast<std::uint32_t>(next);
        const std::uint32_t c = ringStart +
            static_cast<std::uint32_t>(side);
        const std::uint32_t d = ringStart +
            static_cast<std::uint32_t>(next);
        mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
      }
    }
    previousRing = ringStart;
  }

  if (terminal) {
    appendRoundedTip(mesh, branch, previousRing, ringSides,
                     tangent, frameNormal, terminalRadius);
  } else {
    const std::uint32_t center =
        static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(makeVertex(
        add(branch.end, multiply(tangent,
                                 std::max(branch.endRadius * 0.65f, 0.006f))),
        tangent, branch));
    for (int side = 0; side < ringSides; ++side) {
      const int next = (side + 1) % ringSides;
      mesh.indices.insert(mesh.indices.end(), {
          center,
          previousRing + static_cast<std::uint32_t>(side),
          previousRing + static_cast<std::uint32_t>(next),
      });
    }
  }
}

}  // namespace

CoralSkeleton CoralSkeletonGenerator::generate(
    CoralVariantType type, std::uint64_t variantSeed) const {
  return SkeletonBuildContext(type, variantSeed).build();
}

CoralLodBudget CoralMeshBuilder::budget(std::uint32_t lodLevel) {
  switch (lodLevel) {
    case 0: return {72, 220, 4500, 6000, 8};
    case 1: return {36, 90, 1600, 2500, 6};
    default: return {14, 36, 600, 1000, 5};
  }
}

CoralMeshData CoralMeshBuilder::build(
    const CoralSkeleton& skeleton, std::uint32_t lodLevel) const {
  lodLevel = std::min(lodLevel, 2U);
  const CoralLodBudget limits = budget(lodLevel);
  const std::array<std::size_t, 3> targetBranches{{38, 20, 9}};
  const std::array<std::size_t, 3> targetIntervals{{150, 66, 27}};
  const std::array<std::uint32_t, 3> maximumLevel{{5, 3, 2}};
  const std::vector<std::uint32_t> selectedBranches = selectBranches(
      skeleton, std::min(targetBranches[lodLevel], limits.maxBranches),
      maximumLevel[lodLevel]);
  std::vector<bool> selected(skeleton.branches.size(), false);
  for (const std::uint32_t branch : selectedBranches) {
    selected[branch] = true;
  }

  std::vector<int> intervals(selectedBranches.size(), 3);
  std::size_t intervalCount = selectedBranches.size() * 3U;
  std::vector<std::size_t> priority(selectedBranches.size());
  std::iota(priority.begin(), priority.end(), 0U);
  std::stable_sort(priority.begin(), priority.end(),
      [&skeleton, &selectedBranches](std::size_t lhs, std::size_t rhs) {
        return skeleton.branches[selectedBranches[lhs]].significance >
            skeleton.branches[selectedBranches[rhs]].significance;
      });
  const int maximumIntervalsPerBranch = lodLevel == 0U ? 6 :
      (lodLevel == 1U ? 4 : 3);
  while (intervalCount < std::min(
             targetIntervals[lodLevel], limits.maxAxialIntervals)) {
    bool added = false;
    for (const std::size_t index : priority) {
      if (intervals[index] < maximumIntervalsPerBranch &&
          intervalCount < std::min(
              targetIntervals[lodLevel], limits.maxAxialIntervals)) {
        ++intervals[index];
        ++intervalCount;
        added = true;
      }
    }
    if (!added) {
      break;
    }
  }

  CoralMeshData result;
  for (std::size_t index = 0; index < selectedBranches.size(); ++index) {
    const std::uint32_t branchIndex = selectedBranches[index];
    appendBranch(result, skeleton, skeleton.branches[branchIndex],
                 intervals[index], limits.ringSides,
                 !hasSelectedChild(skeleton, selected, branchIndex));
  }
  result.stats.logicalBranches = selectedBranches.size();
  result.stats.axialIntervals = intervalCount;
  result.stats.triangles = result.indices.size() / 3U;
  result.stats.vertices = result.vertices.size();
  assert(result.stats.logicalBranches <= limits.maxBranches);
  assert(result.stats.axialIntervals <= limits.maxAxialIntervals);
  assert(result.stats.triangles <= limits.maxTriangles);
  assert(result.stats.vertices <= limits.maxVertices);
  return result;
}

CoralMeshSet::CoralMeshSet(std::uint64_t coralSeed) {
  CoralSkeletonGenerator skeletonGenerator;
  CoralMeshBuilder meshBuilder;
  for (std::size_t typeIndex = 0; typeIndex < kTypeCount; ++typeIndex) {
    for (std::size_t meshIndex = 0;
         meshIndex < kVariantsPerType; ++meshIndex) {
      CoralMeshVariant& meshVariant =
          variants_[typeIndex * kVariantsPerType + meshIndex];
      meshVariant.type = static_cast<CoralVariantType>(typeIndex);
      meshVariant.meshVariantIndex = static_cast<std::uint32_t>(meshIndex);
      meshVariant.variantSeed = variantSeed(coralSeed, typeIndex, meshIndex);
      const CoralSkeleton skeleton = skeletonGenerator.generate(
          meshVariant.type, meshVariant.variantSeed);
      for (std::size_t lod = 0; lod < kLodCount; ++lod) {
        meshVariant.lods[lod] = meshBuilder.build(
            skeleton, static_cast<std::uint32_t>(lod));
#ifndef NDEBUG
        const CoralMeshStats& stats = meshVariant.lods[lod].stats;
        std::clog << "World11 coral type=" << typeIndex
                  << " variant=" << meshIndex << " lod=" << lod
                  << " branches=" << stats.logicalBranches
                  << " intervals=" << stats.axialIntervals
                  << " triangles=" << stats.triangles
                  << " vertices=" << stats.vertices << '\n';
#endif
      }
    }
  }
}

const CoralMeshVariant& CoralMeshSet::variant(
    std::size_t variantType, std::size_t meshVariantIndex) const {
  return variants_[
      (variantType % kTypeCount) * kVariantsPerType +
      (meshVariantIndex % kVariantsPerType)];
}

std::uint64_t CoralMeshSet::variantSeed(
    std::uint64_t coralSeed, std::size_t variantType,
    std::size_t meshVariantIndex) {
  const std::uint64_t key =
      (static_cast<std::uint64_t>(variantType) << 32U) |
      static_cast<std::uint64_t>(meshVariantIndex);
  return world::World11DecorGenerator::stableHash(
      world::World11DecorGenerator::stableHash(
          coralSeed, 0x434F52414C4D4553ULL), key);
}

}  // namespace hg::render::gl33
