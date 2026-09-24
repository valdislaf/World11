#pragma once

#include <cstdint>

namespace hg::world {

struct World11FishVec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct World11FishMovementVolume {
  World11FishVec3 center{0.0f, 3.5f, -42.0f};
  float horizontalRadius = 16.0f;
  float surfaceClearance = 0.5f;
  float seabedClearance = 0.4f;
  float fishHalfHeight = 0.65f;
  /// <summary>Highest allowed center height above the seabed; reef fish stay low.</summary>
  float maximumSeabedHeight = 1.0e6f;
};

struct World11FishState {
  World11FishVec3 position;
  World11FishVec3 velocity;
  World11FishVec3 desiredDirection;
  float tailPhase = 0.0f;
  std::uint64_t trajectoryEventIndex = 0;
};

/// <summary>
/// Deterministic fixed-step steering trajectory for the World 11 test fish.
/// Direction changes are derived exclusively from (seed, event index), while
/// sample() accepts absolute simulation time so render FPS cannot change the
/// resulting route.
/// </summary>
class World11FishTrajectory final {
public:
  static constexpr double kFixedTimeStep = 1.0 / 120.0;
  static constexpr float kMaximumSpeed = 2.2f;
  static constexpr float kMaximumAcceleration = 0.90f;
  static constexpr float kMaximumTurnRate = 0.82f;

  explicit World11FishTrajectory(
      std::uint64_t trajectorySeed = 0x574F524C44313146ULL,
      World11FishVec3 initialPosition = {-4.0f, 3.5f, -40.0f},
      World11FishMovementVolume movementVolume = {});

  /// <summary>Returns the deterministic state at absolute simulation time.</summary>
  World11FishState sample(double simulationTime);
  void reset();

  std::uint64_t trajectorySeed() const;
  const World11FishVec3& initialPosition() const;
  const World11FishMovementVolume& movementVolume() const;

private:
  struct SimulationState {
    World11FishState fish;
    double time = 0.0;
    double nextEventTime = 0.0;
    float targetSpeed = 1.2f;
  };

  static std::uint64_t stableHash(std::uint64_t seed, std::uint64_t value);
  static float hashUnit(std::uint64_t value);
  void configureEvent(SimulationState& state, std::uint64_t eventIndex) const;
  void advance(SimulationState& state, double deltaTime) const;
  void integrate(SimulationState& state, float deltaTime) const;
  void enforceBounds(SimulationState& state) const;

  std::uint64_t trajectorySeed_;
  World11FishVec3 initialPosition_;
  World11FishMovementVolume movementVolume_;
  SimulationState fixedState_;
  std::uint64_t simulatedTicks_ = 0;
};

}  // namespace hg::world
