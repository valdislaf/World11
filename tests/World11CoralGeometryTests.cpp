#include "render/gl33/World11CoralGeometry.hpp"
#include "world/World11DecorGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

using hg::render::gl33::CoralMeshData;
using hg::render::gl33::CoralMeshSet;
using hg::render::gl33::CoralSkeleton;
using hg::render::gl33::CoralSkeletonGenerator;
using hg::render::gl33::CoralVariantType;

int fail(const char* message) {
  std::cerr << "World11 coral geometry test failed: " << message << '\n';
  return 1;
}

bool sameMesh(const CoralMeshData& lhs, const CoralMeshData& rhs) {
  if (lhs.vertices.size() != rhs.vertices.size() ||
      lhs.indices != rhs.indices ||
      lhs.stats.logicalBranches != rhs.stats.logicalBranches ||
      lhs.stats.axialIntervals != rhs.stats.axialIntervals ||
      lhs.stats.triangles != rhs.stats.triangles ||
      lhs.stats.vertices != rhs.stats.vertices) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.vertices.size(); ++index) {
    for (int component = 0; component < 3; ++component) {
      if (lhs.vertices[index].position[component] !=
              rhs.vertices[index].position[component] ||
          lhs.vertices[index].normal[component] !=
              rhs.vertices[index].normal[component] ||
          lhs.vertices[index].coralData[component] !=
              rhs.vertices[index].coralData[component]) {
        return false;
      }
    }
  }
  return true;
}

bool validSkeleton(const CoralSkeleton& skeleton) {
  if (skeleton.branches.empty() || skeleton.rootRadius < 0.09f ||
      skeleton.rootRadius > 0.14f || skeleton.branches[0].end.y > 0.25f) {
    std::cerr << "root branches=" << skeleton.branches.size()
              << " radius=" << skeleton.rootRadius
              << " baseY=" << skeleton.branches[0].end.y << '\n';
    return false;
  }
  std::size_t mainDirections = 0;
  std::uint32_t maximumLevel = 0;
  float minimumX = 1000.0f;
  float maximumX = -1000.0f;
  float minimumZ = 1000.0f;
  float maximumZ = -1000.0f;
  for (const auto& branch : skeleton.branches) {
    maximumLevel = std::max(maximumLevel, branch.level);
    mainDirections += branch.level == 1U ? 1U : 0U;
    minimumX = std::min(minimumX, std::min(branch.start.x, branch.end.x));
    maximumX = std::max(maximumX, std::max(branch.start.x, branch.end.x));
    minimumZ = std::min(minimumZ, std::min(branch.start.z, branch.end.z));
    maximumZ = std::max(maximumZ, std::max(branch.start.z, branch.end.z));
    if (branch.endRadius <= 0.0f || branch.startRadius <= 0.0f ||
        branch.end.y < -0.02f) {
      std::cerr << "invalid branch level=" << branch.level
                << " endY=" << branch.end.y << '\n';
      return false;
    }
  }
  const float crownWidth = std::max(maximumX - minimumX, maximumZ - minimumZ);
  if (mainDirections < 3U || mainDirections > 6U || maximumLevel < 3U ||
      crownWidth < 0.78f || crownWidth > 1.45f) {
    std::cerr << "shape main=" << mainDirections
              << " maxLevel=" << maximumLevel
              << " width=" << crownWidth << '\n';
    return false;
  }

  constexpr float exponent = 2.4f;
  for (const auto& group : skeleton.areaGroups) {
    const std::size_t maximumChildren = group.parentBranch == 0U ? 6U : 3U;
    if (group.childBranches.size() < 2U ||
        group.childBranches.size() > maximumChildren ||
        group.areaBudget < 0.82f ||
        group.areaBudget > 0.94f) {
      std::cerr << "group children=" << group.childBranches.size()
                << " budget=" << group.areaBudget << '\n';
      return false;
    }
    const float parentRadius =
        skeleton.branches[group.parentBranch].endRadius;
    const float parentArea = std::pow(parentRadius, exponent);
    float childArea = 0.0f;
    for (const std::uint32_t child : group.childBranches) {
      childArea += std::pow(
          skeleton.branches[child].startRadius, exponent);
    }
    if (childArea > 0.95f * parentArea + 1.0e-5f ||
        std::abs(childArea - group.areaBudget * parentArea) > 1.0e-4f) {
      std::cerr << "area child=" << childArea
                << " parent=" << parentArea
                << " budget=" << group.areaBudget << '\n';
      return false;
    }
  }
  return true;
}

bool validMesh(const CoralMeshData& mesh, std::uint32_t lod) {
  const auto budget = hg::render::gl33::CoralMeshBuilder::budget(lod);
  if (mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.indices.size() % 3U != 0U ||
      mesh.stats.logicalBranches > budget.maxBranches ||
      mesh.stats.axialIntervals > budget.maxAxialIntervals ||
      mesh.stats.triangles > budget.maxTriangles ||
      mesh.stats.vertices > budget.maxVertices) {
    return false;
  }
  for (const std::uint32_t index : mesh.indices) {
    if (index >= mesh.vertices.size()) {
      return false;
    }
  }
  for (const auto& vertex : mesh.vertices) {
    const float normalLength = std::sqrt(
        vertex.normal[0] * vertex.normal[0] +
        vertex.normal[1] * vertex.normal[1] +
        vertex.normal[2] * vertex.normal[2]);
    if (normalLength < 0.96f || normalLength > 1.04f ||
        vertex.coralData[0] < 0.0f || vertex.coralData[0] > 1.0f ||
        vertex.coralData[1] < 0.0f || vertex.coralData[1] > 1.0f) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const hg::world::World11DecorGenerator decorGenerator;
  const std::uint64_t coralSeed = decorGenerator.seeds().coralSeed;
  const CoralMeshSet first(coralSeed);
  const CoralMeshSet second(coralSeed);
  CoralSkeletonGenerator skeletonGenerator;

  for (std::size_t type = 0; type < CoralMeshSet::kTypeCount; ++type) {
    for (std::size_t variant = 0;
         variant < CoralMeshSet::kVariantsPerType; ++variant) {
      const std::uint64_t seed = CoralMeshSet::variantSeed(
          coralSeed, type, variant);
      const CoralSkeleton skeleton = skeletonGenerator.generate(
          static_cast<CoralVariantType>(type), seed);
      if (!validSkeleton(skeleton)) {
        std::cerr << "type=" << type << " variant=" << variant << '\n';
        return fail("invalid recursive skeleton or area conservation");
      }
      const auto& firstVariant = first.variant(type, variant);
      const auto& secondVariant = second.variant(type, variant);
      if (firstVariant.variantSeed != seed ||
          secondVariant.variantSeed != seed) {
        return fail("variant seed is not stable");
      }
      for (std::uint32_t lod = 0; lod < CoralMeshSet::kLodCount; ++lod) {
        if (!validMesh(firstVariant.lods[lod], lod) ||
            !sameMesh(firstVariant.lods[lod], secondVariant.lods[lod])) {
          return fail("LOD mesh is invalid or non-deterministic");
        }
        if (lod != 0U &&
            firstVariant.lods[lod].stats.logicalBranches >
                firstVariant.lods[lod - 1U].stats.logicalBranches) {
          return fail("LOD simplification increased branch count");
        }
      }
    }
  }

  std::cout << "World11 coral geometry tests passed\n";
  return 0;
}
