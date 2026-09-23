#include "world/World11DecorGenerator.hpp"
#include "world/World11Seabed.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using hg::world::World11DecorChunk;
using hg::world::World11DecorGenerator;
using hg::world::World11DecorInstance;
using hg::world::World11DecorSeeds;
using hg::world::World11DecorType;

bool sameInstance(const World11DecorInstance& lhs,
                  const World11DecorInstance& rhs) {
  return lhs.type == rhs.type && lhs.x == rhs.x && lhs.y == rhs.y &&
      lhs.z == rhs.z && lhs.normalX == rhs.normalX &&
      lhs.normalY == rhs.normalY && lhs.normalZ == rhs.normalZ &&
      lhs.width == rhs.width && lhs.height == rhs.height &&
      lhs.depth == rhs.depth && lhs.yaw == rhs.yaw &&
      lhs.phase == rhs.phase && lhs.amplitude == rhs.amplitude &&
      lhs.red == rhs.red && lhs.green == rhs.green &&
      lhs.blue == rhs.blue && lhs.variant == rhs.variant &&
      lhs.stableId == rhs.stableId;
}

bool sameInstances(const std::vector<World11DecorInstance>& lhs,
                   const std::vector<World11DecorInstance>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    if (!sameInstance(lhs[index], rhs[index])) {
      return false;
    }
  }
  return true;
}

std::uint64_t quantized(float value) {
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(
      std::llround(static_cast<double>(value) * 4096.0)));
}

std::uint64_t fingerprint(const World11DecorGenerator& generator,
                          World11DecorType type) {
  std::uint64_t value = 0xCBF29CE484222325ULL;
  for (int chunkZ = -3; chunkZ <= 3; ++chunkZ) {
    for (int chunkX = -3; chunkX <= 3; ++chunkX) {
      const World11DecorChunk chunk = generator.generateChunk(chunkX, chunkZ);
      const std::vector<World11DecorInstance>* instances = &chunk.seaweed;
      if (type == World11DecorType::coral) {
        instances = &chunk.coral;
      } else if (type == World11DecorType::bubble) {
        instances = &chunk.bubbles;
      }
      value = World11DecorGenerator::stableHash(
          value, World11DecorGenerator::chunkKey(chunkX, chunkZ));
      value = World11DecorGenerator::stableHash(
          value, static_cast<std::uint64_t>(instances->size()));
      for (const World11DecorInstance& instance : *instances) {
        value = World11DecorGenerator::stableHash(value, instance.stableId);
        value = World11DecorGenerator::stableHash(value, instance.variant);
        for (const float property : {
                 instance.x, instance.y, instance.z,
                 instance.normalX, instance.normalY, instance.normalZ,
                 instance.width, instance.height, instance.depth,
                 instance.yaw, instance.phase, instance.amplitude,
                 instance.red, instance.green, instance.blue}) {
          value = World11DecorGenerator::stableHash(
              value, quantized(property));
        }
      }
    }
  }
  return value;
}

bool validateGrounding(const World11DecorGenerator& generator,
                       std::string& message) {
  std::size_t checked = 0;
  bool crossesParentBoundary = false;
  std::unordered_set<std::uint64_t> stableIds;
  for (int chunkZ = -2; chunkZ <= 2; ++chunkZ) {
    for (int chunkX = -2; chunkX <= 2; ++chunkX) {
      const World11DecorChunk chunk = generator.generateChunk(chunkX, chunkZ);
      for (const auto* instances : {
               &chunk.seaweed, &chunk.coral, &chunk.bubbles}) {
        for (const World11DecorInstance& instance : *instances) {
          if (!stableIds.insert(instance.stableId).second) {
            message = "duplicate instance id generated across parent chunks";
            return false;
          }
          const float minimumX = static_cast<float>(chunkX) *
              World11DecorGenerator::kChunkSize;
          const float minimumZ = static_cast<float>(chunkZ) *
              World11DecorGenerator::kChunkSize;
          const float maximumX = minimumX + World11DecorGenerator::kChunkSize;
          const float maximumZ = minimumZ + World11DecorGenerator::kChunkSize;
          crossesParentBoundary = crossesParentBoundary ||
              instance.x < minimumX || instance.x >= maximumX ||
              instance.z < minimumZ || instance.z >= maximumZ;
          const float expectedY = hg::world::world11SeabedHeight(
              instance.x, instance.z);
          if (std::abs(instance.y - expectedY) > 1.0e-5f) {
            message = "instance base is not on World 11 seabed";
            return false;
          }
          const float normalLength = std::sqrt(
              instance.normalX * instance.normalX +
              instance.normalY * instance.normalY +
              instance.normalZ * instance.normalZ);
          if (std::abs(normalLength - 1.0f) > 1.0e-4f ||
              instance.normalY < 0.78f) {
            message = "invalid or too-steep seabed normal accepted";
            return false;
          }
          if (instance.type != World11DecorType::bubble &&
              instance.y + instance.height >=
              World11DecorGenerator::kWaterSurfaceY - 0.35f) {
            message = "decor reaches too close to the water surface";
            return false;
          }
          ++checked;
        }
      }
    }
  }
  if (checked == 0) {
    message = "test area unexpectedly contains no decor";
    return false;
  }
  if (!crossesParentBoundary) {
    message = "no cluster instance crossed its parent chunk boundary";
    return false;
  }
  return true;
}

int fail(const std::string& message) {
  std::cerr << "World11DecorGenerator test failed: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  if (World11DecorGenerator::chunkCoordinate(0.0f) != 0 ||
      World11DecorGenerator::chunkCoordinate(31.999f) != 0 ||
      World11DecorGenerator::chunkCoordinate(32.0f) != 1 ||
      World11DecorGenerator::chunkCoordinate(-0.001f) != -1 ||
      World11DecorGenerator::chunkCoordinate(-32.0f) != -1 ||
      World11DecorGenerator::chunkCoordinate(-32.001f) != -2) {
    return fail("mathematical floor chunk mapping is incorrect");
  }

  const World11DecorGenerator defaultGenerator;
  std::size_t nearbyGroupedBubbles = 0;
  std::size_t nearbyIsolatedBubbles = 0;
  for (int chunkZ = -1; chunkZ <= 1; ++chunkZ) {
    for (int chunkX = -1; chunkX <= 1; ++chunkX) {
      const World11DecorChunk chunk = defaultGenerator.generateChunk(
          chunkX, chunkZ);
      for (const World11DecorInstance& bubble : chunk.bubbles) {
        if (bubble.variant == 0U) {
          ++nearbyGroupedBubbles;
        } else {
          ++nearbyIsolatedBubbles;
        }
      }
    }
  }
  if (nearbyGroupedBubbles < 8 || nearbyIsolatedBubbles < 2) {
    return fail("default entry area lacks grouped or isolated bubbles");
  }

  const World11DecorSeeds baseSeeds{
      0x1020304050607080ULL,
      0x1122334455667788ULL,
      0x8877665544332211ULL,
      0x3141592653589793ULL,
  };
  const World11DecorGenerator first(baseSeeds);
  const World11DecorGenerator second(baseSeeds);
  first.generateChunk(4, -7);
  const World11DecorChunk firstChunk = first.generateChunk(-2, 3);
  const World11DecorChunk secondChunk = second.generateChunk(-2, 3);
  if (!sameInstances(firstChunk.seaweed, secondChunk.seaweed) ||
      !sameInstances(firstChunk.coral, secondChunk.coral)) {
    return fail("same seeds did not reproduce the same chunk");
  }

  const std::uint64_t baseSeaweed = fingerprint(
      first, World11DecorType::seaweed);
  const std::uint64_t baseCoral = fingerprint(
      first, World11DecorType::coral);
  const std::uint64_t baseBubbles = fingerprint(
      first, World11DecorType::bubble);

  World11DecorSeeds changedSeaweedSeeds = baseSeeds;
  changedSeaweedSeeds.seaweedSeed ^= 0xD1B54A32D192ED03ULL;
  const World11DecorGenerator changedSeaweed(changedSeaweedSeeds);
  if (fingerprint(changedSeaweed, World11DecorType::seaweed) == baseSeaweed ||
      fingerprint(changedSeaweed, World11DecorType::coral) != baseCoral ||
      fingerprint(changedSeaweed, World11DecorType::bubble) != baseBubbles) {
    return fail("seaweedSeed did not affect only seaweed");
  }

  World11DecorSeeds changedCoralSeeds = baseSeeds;
  changedCoralSeeds.coralSeed ^= 0x94D049BB133111EBULL;
  const World11DecorGenerator changedCoral(changedCoralSeeds);
  if (fingerprint(changedCoral, World11DecorType::coral) == baseCoral ||
      fingerprint(changedCoral, World11DecorType::seaweed) != baseSeaweed ||
      fingerprint(changedCoral, World11DecorType::bubble) != baseBubbles) {
    return fail("coralSeed did not affect only coral");
  }

  World11DecorSeeds changedBubbleSeeds = baseSeeds;
  changedBubbleSeeds.bubbleSeed ^= 0xDB4F0B9175AE2165ULL;
  const World11DecorGenerator changedBubbles(changedBubbleSeeds);
  if (fingerprint(changedBubbles, World11DecorType::bubble) == baseBubbles ||
      fingerprint(changedBubbles, World11DecorType::seaweed) != baseSeaweed ||
      fingerprint(changedBubbles, World11DecorType::coral) != baseCoral) {
    return fail("bubbleSeed did not affect only bubbles");
  }

  World11DecorSeeds changedWorldSeeds = baseSeeds;
  changedWorldSeeds.worldSeed += 1U;
  const World11DecorGenerator changedWorld(changedWorldSeeds);
  if (fingerprint(changedWorld, World11DecorType::seaweed) == baseSeaweed ||
      fingerprint(changedWorld, World11DecorType::coral) == baseCoral ||
      fingerprint(changedWorld, World11DecorType::bubble) == baseBubbles) {
    return fail("worldSeed did not rebuild every decor type");
  }

  std::string groundingMessage;
  if (!validateGrounding(first, groundingMessage)) {
    return fail(groundingMessage);
  }

  constexpr std::uint64_t kExpectedSeaweedFingerprint =
      0xED9B0242A602AFECULL;
  constexpr std::uint64_t kExpectedCoralFingerprint =
      0xA8511C081C82D5E8ULL;
  constexpr std::uint64_t kExpectedBubbleFingerprint =
      0xEC633B0C7B587F8AULL;
  if (baseSeaweed != kExpectedSeaweedFingerprint ||
      baseCoral != kExpectedCoralFingerprint ||
      baseBubbles != kExpectedBubbleFingerprint) {
    std::cerr << "Golden fingerprints need update: seaweed=0x" << std::hex
              << baseSeaweed << " coral=0x" << baseCoral
              << " bubbles=0x" << baseBubbles << '\n';
    return 2;
  }

  std::cout << "World11DecorGenerator tests passed\n";
  return 0;
}
