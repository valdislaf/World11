#pragma once

#include <cstdint>
#include <vector>

namespace hg::world {

enum class World11DecorType : std::uint8_t {
  seaweed,
  coral,
  bubble,
};

struct World11DecorSeeds {
  std::uint64_t worldSeed = 0x574F524C443131ULL;
  std::uint64_t seaweedSeed = 0;
  std::uint64_t coralSeed = 0;
  std::uint64_t bubbleSeed = 0;
};

struct World11DecorInstance {
  World11DecorType type = World11DecorType::seaweed;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float normalX = 0.0f;
  float normalY = 1.0f;
  float normalZ = 0.0f;
  float width = 1.0f;
  float height = 1.0f;
  float depth = 1.0f;
  float yaw = 0.0f;
  float phase = 0.0f;
  float amplitude = 0.0f;
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  std::uint32_t variant = 0;
  std::uint64_t stableId = 0;
};

struct World11DecorChunk {
  int chunkX = 0;
  int chunkZ = 0;
  std::vector<World11DecorInstance> seaweed;
  std::vector<World11DecorInstance> coral;
  std::vector<World11DecorInstance> bubbles;
};

/// <summary>
/// OpenGL-independent, order-independent generator for World 11 decor chunks.
/// All random streams are derived from stable 64-bit integer seeds.
/// </summary>
class World11DecorGenerator final {
public:
  static constexpr float kChunkSize = 32.0f;
  static constexpr float kWaterSurfaceY = 12.5f;

  explicit World11DecorGenerator(World11DecorSeeds seeds = {});

  void setSeeds(World11DecorSeeds seeds);
  const World11DecorSeeds& seeds() const;
  World11DecorChunk generateChunk(int chunkX, int chunkZ) const;

  static int chunkCoordinate(float worldCoordinate);
  static std::uint64_t stableHash(std::uint64_t seed, std::uint64_t value);
  static std::uint64_t chunkKey(int chunkX, int chunkZ);

private:
  World11DecorSeeds seeds_;
};

}  // namespace hg::world
