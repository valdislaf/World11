#include "world/World11DecorGenerator.hpp"

#include "world/World11Seabed.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace hg::world {

namespace {

constexpr std::uint64_t kSeaweedSalt = 0x5345415745454431ULL;
constexpr std::uint64_t kCoralSalt = 0x434F52414C5F3131ULL;
constexpr std::uint64_t kBubbleSalt = 0x425542424C455331ULL;
constexpr std::uint64_t kClusterSalt = 0x434C555354455231ULL;
constexpr std::uint64_t kInstanceSalt = 0x494E5354414E4345ULL;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr std::int64_t kPositionUnitsPerMeter = 256;
constexpr std::int64_t kChunkSizeUnits =
    static_cast<std::int64_t>(World11DecorGenerator::kChunkSize) *
    kPositionUnitsPerMeter;

std::uint64_t splitMix64(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

class Pcg32 final {
public:
  explicit Pcg32(std::uint64_t seed, std::uint64_t stream) {
    increment_ = (stream << 1U) | 1U;
    next();
    state_ += seed;
    next();
  }

  std::uint32_t next() {
    const std::uint64_t oldState = state_;
    state_ = oldState * 6364136223846793005ULL + increment_;
    const std::uint32_t xorShifted = static_cast<std::uint32_t>(
        ((oldState >> 18U) ^ oldState) >> 27U);
    const std::uint32_t rotation = static_cast<std::uint32_t>(oldState >> 59U);
    return (xorShifted >> rotation) |
        (xorShifted << ((0U - rotation) & 31U));
  }

  std::uint32_t bounded(std::uint32_t bound) {
    const std::uint32_t threshold = (0U - bound) % bound;
    for (;;) {
      const std::uint32_t value = next();
      if (value >= threshold) {
        return value % bound;
      }
    }
  }

  float unit() {
    return static_cast<float>(next() >> 8U) * (1.0f / 16777216.0f);
  }

  float range(float minimum, float maximum) {
    return minimum + (maximum - minimum) * unit();
  }

private:
  std::uint64_t state_ = 0;
  std::uint64_t increment_ = 1;
};

std::uint64_t signedCoordinateBits(int coordinate) {
  return static_cast<std::uint64_t>(
      static_cast<std::uint32_t>(static_cast<std::int32_t>(coordinate)));
}

std::uint64_t typeSeed(const World11DecorSeeds& seeds,
                       World11DecorType type) {
  std::uint64_t localSeed = seeds.seaweedSeed;
  std::uint64_t salt = kSeaweedSalt;
  if (type == World11DecorType::coral) {
    localSeed = seeds.coralSeed;
    salt = kCoralSalt;
  } else if (type == World11DecorType::bubble) {
    localSeed = seeds.bubbleSeed;
    salt = kBubbleSalt;
  }
  std::uint64_t result = World11DecorGenerator::stableHash(seeds.worldSeed, salt);
  return World11DecorGenerator::stableHash(result, localSeed);
}

std::uint64_t makeChunkSeed(std::uint64_t decorTypeSeed,
                            int chunkX, int chunkZ) {
  std::uint64_t seed = World11DecorGenerator::stableHash(
      decorTypeSeed, signedCoordinateBits(chunkX));
  return World11DecorGenerator::stableHash(seed, signedCoordinateBits(chunkZ));
}

std::uint64_t makeClusterSeed(std::uint64_t chunkSeed,
                              std::uint32_t clusterIndex) {
  return World11DecorGenerator::stableHash(
      World11DecorGenerator::stableHash(chunkSeed, kClusterSalt),
      clusterIndex);
}

std::uint64_t makeInstanceSeed(std::uint64_t clusterSeed,
                               std::uint32_t instanceIndex) {
  return World11DecorGenerator::stableHash(
      World11DecorGenerator::stableHash(clusterSeed, kInstanceSalt),
      instanceIndex);
}

void seabedFrame(float x, float z, float& y,
                  float& normalX, float& normalY, float& normalZ) {
  constexpr float sampleOffset = 0.45f;
  y = world11SeabedHeight(x, z);
  const float derivativeX =
      (world11SeabedHeight(x + sampleOffset, z) -
       world11SeabedHeight(x - sampleOffset, z)) /
      (2.0f * sampleOffset);
  const float derivativeZ =
      (world11SeabedHeight(x, z + sampleOffset) -
       world11SeabedHeight(x, z - sampleOffset)) /
      (2.0f * sampleOffset);
  normalX = -derivativeX;
  normalY = 1.0f;
  normalZ = -derivativeZ;
  const float inverseLength = 1.0f / std::sqrt(
      normalX * normalX + normalY * normalY + normalZ * normalZ);
  normalX *= inverseLength;
  normalY *= inverseLength;
  normalZ *= inverseLength;
}

bool suitableSeabed(const World11DecorInstance& instance) {
  return instance.normalY >= 0.78f &&
      instance.y < World11DecorGenerator::kWaterSurfaceY - 2.0f &&
      instance.y + instance.height <
          World11DecorGenerator::kWaterSurfaceY - 0.35f;
}

std::int64_t clusterCenterUnits(int chunkCoordinate, Pcg32& random) {
  return static_cast<std::int64_t>(chunkCoordinate) * kChunkSizeUnits +
      static_cast<std::int64_t>(random.bounded(
          static_cast<std::uint32_t>(kChunkSizeUnits)));
}

void placeInCluster(World11DecorInstance& instance, Pcg32& random,
                    std::int64_t centerX, std::int64_t centerZ,
                    std::int32_t clusterRadius) {
  std::int32_t offsetX = 0;
  std::int32_t offsetZ = 0;
  const std::uint32_t diameter =
      static_cast<std::uint32_t>(clusterRadius * 2 + 1);
  do {
    offsetX = static_cast<std::int32_t>(random.bounded(diameter)) -
        clusterRadius;
    offsetZ = static_cast<std::int32_t>(random.bounded(diameter)) -
        clusterRadius;
  } while (static_cast<std::int64_t>(offsetX) * offsetX +
               static_cast<std::int64_t>(offsetZ) * offsetZ >
           static_cast<std::int64_t>(clusterRadius) * clusterRadius);

  const std::int64_t centerBias = random.bounded(65536U);
  offsetX = static_cast<std::int32_t>(
      static_cast<std::int64_t>(offsetX) * centerBias / 65535);
  offsetZ = static_cast<std::int32_t>(
      static_cast<std::int64_t>(offsetZ) * centerBias / 65535);
  instance.x = static_cast<float>(centerX + offsetX) /
      static_cast<float>(kPositionUnitsPerMeter);
  instance.z = static_cast<float>(centerZ + offsetZ) /
      static_cast<float>(kPositionUnitsPerMeter);
  seabedFrame(instance.x, instance.z, instance.y,
               instance.normalX, instance.normalY, instance.normalZ);
}

void generateSeaweed(const World11DecorSeeds& seeds, int chunkX, int chunkZ,
                     std::vector<World11DecorInstance>& output) {
  const std::uint64_t chunkSeed = makeChunkSeed(
      typeSeed(seeds, World11DecorType::seaweed), chunkX, chunkZ);
  Pcg32 chunkRandom(chunkSeed, kSeaweedSalt);
  const std::uint32_t clusterCount = chunkRandom.bounded(4U);
  for (std::uint32_t clusterIndex = 0;
       clusterIndex < clusterCount; ++clusterIndex) {
    const std::uint64_t clusterSeed = makeClusterSeed(chunkSeed, clusterIndex);
    Pcg32 clusterRandom(clusterSeed, kSeaweedSalt ^ clusterIndex);
    const std::int64_t centerX = clusterCenterUnits(chunkX, clusterRandom);
    const std::int64_t centerZ = clusterCenterUnits(chunkZ, clusterRandom);
    const std::int32_t radius = 2 * static_cast<std::int32_t>(
        kPositionUnitsPerMeter) + static_cast<std::int32_t>(
        clusterRandom.bounded(4U * static_cast<std::uint32_t>(
            kPositionUnitsPerMeter) + 1U));
    const std::uint32_t instanceCount = 6U + clusterRandom.bounded(19U);
    for (std::uint32_t instanceIndex = 0;
         instanceIndex < instanceCount; ++instanceIndex) {
      const std::uint64_t instanceSeed = makeInstanceSeed(
          clusterSeed, instanceIndex);
      Pcg32 random(instanceSeed, kSeaweedSalt ^ instanceIndex);
      World11DecorInstance instance;
      instance.type = World11DecorType::seaweed;
      instance.stableId = instanceSeed;
      instance.variant = random.bounded(4U);
      instance.width = random.range(0.12f, 0.34f);
      instance.height = random.range(1.5f, 4.7f);
      instance.depth = random.range(-0.18f, 0.18f);
      instance.yaw = random.range(0.0f, kTwoPi);
      instance.phase = random.range(0.0f, kTwoPi);
      instance.amplitude = random.range(0.18f, 0.62f);
      const float hue = random.unit();
      instance.red = 0.018f + 0.025f * hue;
      instance.green = 0.29f + 0.30f * hue;
      instance.blue = 0.10f + 0.16f * (1.0f - hue);
      placeInCluster(instance, random, centerX, centerZ, radius);
      if (suitableSeabed(instance)) {
        output.push_back(instance);
      }
    }
  }
}

void coralPalette(std::uint32_t palette, float tint,
                  float& red, float& green, float& blue) {
  switch (palette % 5U) {
    case 0:
      red = 0.82f + 0.14f * tint;
      green = 0.12f + 0.20f * tint;
      blue = 0.16f + 0.16f * tint;
      break;
    case 1:
      red = 0.92f;
      green = 0.34f + 0.24f * tint;
      blue = 0.18f + 0.12f * tint;
      break;
    case 2:
      red = 0.64f + 0.20f * tint;
      green = 0.18f + 0.12f * tint;
      blue = 0.72f + 0.20f * tint;
      break;
    case 3:
      red = 0.94f;
      green = 0.42f + 0.25f * tint;
      blue = 0.58f + 0.25f * tint;
      break;
    default:
      red = 0.20f + 0.20f * tint;
      green = 0.62f + 0.22f * tint;
      blue = 0.68f + 0.20f * tint;
      break;
  }
}

void generateCoral(const World11DecorSeeds& seeds, int chunkX, int chunkZ,
                   std::vector<World11DecorInstance>& output) {
  const std::uint64_t chunkSeed = makeChunkSeed(
      typeSeed(seeds, World11DecorType::coral), chunkX, chunkZ);
  Pcg32 chunkRandom(chunkSeed, kCoralSalt);
  const std::uint32_t clusterCount = chunkRandom.bounded(4U);
  for (std::uint32_t clusterIndex = 0;
       clusterIndex < clusterCount; ++clusterIndex) {
    const std::uint64_t clusterSeed = makeClusterSeed(chunkSeed, clusterIndex);
    Pcg32 clusterRandom(clusterSeed, kCoralSalt ^ clusterIndex);
    const std::int64_t centerX = clusterCenterUnits(chunkX, clusterRandom);
    const std::int64_t centerZ = clusterCenterUnits(chunkZ, clusterRandom);
    const std::int32_t radius = 384 + static_cast<std::int32_t>(
        clusterRandom.bounded(897U));
    const std::uint32_t palette = clusterRandom.bounded(5U);
    const std::uint32_t colonyVariant = clusterRandom.bounded(4U);
    const std::uint32_t instanceCount = 3U + clusterRandom.bounded(12U);
    for (std::uint32_t instanceIndex = 0;
         instanceIndex < instanceCount; ++instanceIndex) {
      const std::uint64_t instanceSeed = makeInstanceSeed(
          clusterSeed, instanceIndex);
      Pcg32 random(instanceSeed, kCoralSalt ^ instanceIndex);
      World11DecorInstance instance;
      instance.type = World11DecorType::coral;
      instance.stableId = instanceSeed;
      instance.variant = (colonyVariant + random.bounded(2U)) % 4U;
      instance.width = random.range(0.55f, 1.30f);
      instance.height = random.range(0.75f, 2.25f);
      instance.depth = random.range(0.55f, 1.25f);
      instance.yaw = random.range(0.0f, kTwoPi);
      instance.phase = 0.0f;
      instance.amplitude = 0.0f;
      coralPalette(palette, random.unit(),
                   instance.red, instance.green, instance.blue);
      placeInCluster(instance, random, centerX, centerZ, radius);
      if (suitableSeabed(instance)) {
        output.push_back(instance);
      }
    }
  }
}

void configureBubble(World11DecorInstance& instance, Pcg32& random,
                     std::uint64_t instanceSeed, std::uint32_t variant) {
  instance.type = World11DecorType::bubble;
  instance.stableId = instanceSeed;
  instance.variant = variant;
  instance.width = random.range(variant == 0U ? 0.035f : 0.030f,
                                variant == 0U ? 0.085f : 0.068f);
  instance.height = random.range(0.52f, variant == 0U ? 1.18f : 0.92f);
  instance.depth = 1.0f;
  instance.yaw = random.range(0.0f, kTwoPi);
  instance.phase = random.unit();
  instance.amplitude = random.range(0.035f, variant == 0U ? 0.28f : 0.12f);
  instance.red = 0.62f;
  instance.green = 0.94f;
  instance.blue = 1.0f;
}

bool suitableBubbleSource(const World11DecorInstance& instance) {
  return instance.normalY >= 0.78f &&
      instance.y < World11DecorGenerator::kWaterSurfaceY - 0.8f;
}

void generateBubbles(const World11DecorSeeds& seeds, int chunkX, int chunkZ,
                     std::vector<World11DecorInstance>& output) {
  const std::uint64_t chunkSeed = makeChunkSeed(
      typeSeed(seeds, World11DecorType::bubble), chunkX, chunkZ);
  Pcg32 chunkRandom(chunkSeed, kBubbleSalt);

  const std::uint32_t clusterRoll = chunkRandom.bounded(8U);
  const std::uint32_t clusterCount = clusterRoll < 4U
      ? 0U : (clusterRoll == 7U ? 2U : 1U);
  for (std::uint32_t clusterIndex = 0;
       clusterIndex < clusterCount; ++clusterIndex) {
    const std::uint64_t clusterSeed = makeClusterSeed(chunkSeed, clusterIndex);
    Pcg32 clusterRandom(clusterSeed, kBubbleSalt ^ clusterIndex);
    const std::int64_t centerX = clusterCenterUnits(chunkX, clusterRandom);
    const std::int64_t centerZ = clusterCenterUnits(chunkZ, clusterRandom);
    const std::int32_t radius = 102 + static_cast<std::int32_t>(
        clusterRandom.bounded(360U));
    const std::uint32_t instanceCount = 8U + clusterRandom.bounded(21U);
    for (std::uint32_t instanceIndex = 0;
         instanceIndex < instanceCount; ++instanceIndex) {
      const std::uint64_t instanceSeed = makeInstanceSeed(
          clusterSeed, instanceIndex);
      Pcg32 random(instanceSeed, kBubbleSalt ^ instanceIndex);
      World11DecorInstance instance;
      configureBubble(instance, random, instanceSeed, 0U);
      placeInCluster(instance, random, centerX, centerZ, radius);
      if (suitableBubbleSource(instance)) {
        output.push_back(instance);
      }
    }
  }

  const std::uint32_t isolatedCount = chunkRandom.bounded(4U);
  const std::uint64_t isolatedClusterSeed = makeClusterSeed(
      chunkSeed, 0xFFFFU);
  for (std::uint32_t instanceIndex = 0;
       instanceIndex < isolatedCount; ++instanceIndex) {
    const std::uint64_t instanceSeed = makeInstanceSeed(
        isolatedClusterSeed, instanceIndex);
    Pcg32 random(instanceSeed, kBubbleSalt ^ 0x51504C45ULL ^ instanceIndex);
    World11DecorInstance instance;
    configureBubble(instance, random, instanceSeed, 1U);
    const std::int64_t positionX = clusterCenterUnits(chunkX, random);
    const std::int64_t positionZ = clusterCenterUnits(chunkZ, random);
    instance.x = static_cast<float>(positionX) /
        static_cast<float>(kPositionUnitsPerMeter);
    instance.z = static_cast<float>(positionZ) /
        static_cast<float>(kPositionUnitsPerMeter);
    seabedFrame(instance.x, instance.z, instance.y,
                 instance.normalX, instance.normalY, instance.normalZ);
    if (suitableBubbleSource(instance)) {
      output.push_back(instance);
    }
  }
}

}  // namespace

World11DecorGenerator::World11DecorGenerator(World11DecorSeeds seeds) {
  setSeeds(seeds);
}

void World11DecorGenerator::setSeeds(World11DecorSeeds seeds) {
  if (seeds.seaweedSeed == 0) {
    seeds.seaweedSeed = stableHash(seeds.worldSeed, kSeaweedSalt);
  }
  if (seeds.coralSeed == 0) {
    seeds.coralSeed = stableHash(seeds.worldSeed, kCoralSalt);
  }
  if (seeds.bubbleSeed == 0) {
    seeds.bubbleSeed = stableHash(seeds.worldSeed, kBubbleSalt);
  }
  seeds_ = seeds;
}

const World11DecorSeeds& World11DecorGenerator::seeds() const {
  return seeds_;
}

World11DecorChunk World11DecorGenerator::generateChunk(
    int chunkX, int chunkZ) const {
  World11DecorChunk result;
  result.chunkX = chunkX;
  result.chunkZ = chunkZ;
  generateSeaweed(seeds_, chunkX, chunkZ, result.seaweed);
  generateCoral(seeds_, chunkX, chunkZ, result.coral);
  generateBubbles(seeds_, chunkX, chunkZ, result.bubbles);
  return result;
}

int World11DecorGenerator::chunkCoordinate(float worldCoordinate) {
  return static_cast<int>(std::floor(worldCoordinate / kChunkSize));
}

std::uint64_t World11DecorGenerator::stableHash(
    std::uint64_t seed, std::uint64_t value) {
  return splitMix64(seed ^ splitMix64(value));
}

std::uint64_t World11DecorGenerator::chunkKey(int chunkX, int chunkZ) {
  return (signedCoordinateBits(chunkX) << 32U) |
      signedCoordinateBits(chunkZ);
}

}  // namespace hg::world
