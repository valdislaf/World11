#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hg::render::gl33 {

enum class CoralVariantType : std::uint8_t {
  Bushy = 0,
  Fan = 1,
  Tree = 2,
  LowSpreading = 3,
};

struct CoralVec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct CoralSkeletonBranch {
  static constexpr std::uint32_t kNoParent = 0xFFFFFFFFU;

  CoralVec3 start;
  CoralVec3 end;
  CoralVec3 jointCenter;
  CoralVec3 startTangent;
  CoralVec3 endTangent;
  float startRadius = 0.1f;
  float endRadius = 0.08f;
  float branchTint = 0.0f;
  float significance = 0.0f;
  float tipBulbScale = 1.0f;
  std::uint32_t parent = kNoParent;
  std::uint32_t level = 0;
  std::uint32_t outgoingGroup = 0;
  bool terminal = true;
};

struct CoralAreaGroup {
  std::uint32_t parentBranch = 0;
  float areaBudget = 0.9f;
  std::vector<std::uint32_t> childBranches;
};

struct CoralSkeleton {
  CoralVariantType type = CoralVariantType::Bushy;
  std::uint64_t variantSeed = 0;
  float rootRadius = 0.11f;
  std::vector<CoralSkeletonBranch> branches;
  std::vector<CoralAreaGroup> areaGroups;
};

class CoralSkeletonGenerator final {
public:
  CoralSkeleton generate(CoralVariantType type,
                         std::uint64_t variantSeed) const;
};

struct CoralVertex {
  float position[3];
  float normal[3];
  // Normalized height, normalized branch level and per-branch tint.
  float coralData[3];
};

struct CoralMeshStats {
  std::size_t logicalBranches = 0;
  std::size_t axialIntervals = 0;
  std::size_t triangles = 0;
  std::size_t vertices = 0;
};

struct CoralLodBudget {
  std::size_t maxBranches;
  std::size_t maxAxialIntervals;
  std::size_t maxTriangles;
  std::size_t maxVertices;
  int ringSides;
};

struct CoralMeshData {
  std::vector<CoralVertex> vertices;
  std::vector<std::uint32_t> indices;
  CoralMeshStats stats;
};

class CoralMeshBuilder final {
public:
  static CoralLodBudget budget(std::uint32_t lodLevel);
  CoralMeshData build(const CoralSkeleton& skeleton,
                      std::uint32_t lodLevel) const;
};

struct CoralMeshVariant {
  CoralVariantType type = CoralVariantType::Bushy;
  std::uint32_t meshVariantIndex = 0;
  std::uint64_t variantSeed = 0;
  std::array<CoralMeshData, 3> lods;
};

class CoralMeshSet final {
public:
  static constexpr std::size_t kTypeCount = 4;
  static constexpr std::size_t kVariantsPerType = 3;
  static constexpr std::size_t kLodCount = 3;

  explicit CoralMeshSet(std::uint64_t coralSeed);

  const CoralMeshVariant& variant(std::size_t variantType,
                                  std::size_t meshVariantIndex) const;
  static std::uint64_t variantSeed(std::uint64_t coralSeed,
                                   std::size_t variantType,
                                   std::size_t meshVariantIndex);

private:
  std::array<CoralMeshVariant, kTypeCount * kVariantsPerType> variants_;
};

}  // namespace hg::render::gl33
