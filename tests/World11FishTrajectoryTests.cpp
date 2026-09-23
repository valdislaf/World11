#include "world/World11FishTrajectory.hpp"
#include "world/World11Seabed.hpp"
#include "world/World11WaterSurface.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using hg::world::World11FishState;
using hg::world::World11FishTrajectory;
using hg::world::World11FishVec3;

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "World11FishTrajectory test failed: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

float distance(const World11FishVec3& lhs, const World11FishVec3& rhs) {
  const float x = lhs.x - rhs.x;
  const float y = lhs.y - rhs.y;
  const float z = lhs.z - rhs.z;
  return std::sqrt(x * x + y * y + z * z);
}

float vectorLength(const World11FishVec3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

float vectorDot(const World11FishVec3& lhs, const World11FishVec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

bool sameState(const World11FishState& lhs, const World11FishState& rhs,
               float epsilon = 1.0e-5f) {
  return distance(lhs.position, rhs.position) <= epsilon &&
      distance(lhs.velocity, rhs.velocity) <= epsilon &&
      std::abs(lhs.tailPhase - rhs.tailPhase) <= epsilon &&
      lhs.trajectoryEventIndex == rhs.trajectoryEventIndex;
}

void validateBounds(const World11FishTrajectory& trajectory,
                    const World11FishState& state, float time) {
  const auto& volume = trajectory.movementVolume();
  const float offsetX = state.position.x - volume.center.x;
  const float offsetZ = state.position.z - volume.center.z;
  const float radius = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
  if (radius > volume.horizontalRadius + 1.0e-3f) {
    fail("fish escaped the horizontal movement volume");
  }

  const float waterSurface = hg::world::world11WaterSurfaceHeight(
      state.position.x, state.position.z, time);
  if (state.position.y + volume.fishHalfHeight >
      waterSurface - volume.surfaceClearance + 2.0e-3f) {
    fail("fish crossed the actual displaced water-surface clearance");
  }
  const float seabed = hg::world::world11SeabedHeight(
      state.position.x, state.position.z);
  if (state.position.y - volume.fishHalfHeight <
      seabed + volume.seabedClearance - 2.0e-3f) {
    fail("fish crossed the procedural seabed clearance");
  }
}

}  // namespace

int main() {
  constexpr std::uint64_t kSeed = 0x123456789ABCDEF0ULL;
  constexpr double kDuration = 600.0;

  World11FishTrajectory direct(kSeed);
  const World11FishState directFinal = direct.sample(kDuration);

  World11FishTrajectory thirtyFps(kSeed);
  World11FishState thirtyFinal{};
  for (int frame = 0; frame <= 18000; ++frame) {
    thirtyFinal = thirtyFps.sample(static_cast<double>(frame) / 30.0);
  }

  World11FishTrajectory irregularFps(kSeed);
  double irregularTime = 0.0;
  int cadenceIndex = 0;
  constexpr double cadence[] = {
      1.0 / 144.0, 1.0 / 52.0, 1.0 / 91.0, 1.0 / 37.0,
  };
  while (irregularTime < kDuration) {
    irregularTime = std::min(
        kDuration, irregularTime + cadence[cadenceIndex % 4]);
    irregularFps.sample(irregularTime);
    ++cadenceIndex;
  }
  const World11FishState irregularFinal = irregularFps.sample(kDuration);
  if (!sameState(directFinal, thirtyFinal) ||
      !sameState(directFinal, irregularFinal)) {
    fail("absolute-time fixed-step result depends on render cadence");
  }

  World11FishTrajectory route(kSeed);
  std::vector<World11FishVec3> seconds;
  seconds.reserve(601);
  std::uint64_t previousEvent = 0;
  double previousEventTime = 0.0;
  int observedEvents = 0;
  for (int sampleIndex = 0; sampleIndex <= 2400; ++sampleIndex) {
    const float time = static_cast<float>(sampleIndex) * 0.25f;
    const World11FishState state = route.sample(time);
    validateBounds(route, state, time);
    if (sampleIndex % 4 == 0) {
      seconds.push_back(state.position);
    }
    if (state.trajectoryEventIndex != previousEvent) {
      const double interval = static_cast<double>(time) - previousEventTime;
      if (interval < 1.75 || interval > 6.25) {
        fail("hashed direction-change interval is outside 2-6 seconds");
      }
      previousEvent = state.trajectoryEventIndex;
      previousEventTime = time;
      ++observedEvents;
    }
  }
  if (observedEvents < 100) {
    fail("too few direction events during ten-minute simulation");
  }

  World11FishTrajectory dynamics(kSeed);
  World11FishState previous = dynamics.sample(0.0);
  for (std::uint64_t tick = 1; tick <= 72000; ++tick) {
    const double time = static_cast<double>(tick) *
        World11FishTrajectory::kFixedTimeStep;
    const World11FishState current = dynamics.sample(time);
    const float previousSpeed = vectorLength(previous.velocity);
    const float currentSpeed = vectorLength(current.velocity);
    if (currentSpeed > World11FishTrajectory::kMaximumSpeed + 1.0e-4f) {
      fail("maximum speed was exceeded");
    }
    const float acceleration = distance(
        previous.velocity, current.velocity) /
        static_cast<float>(World11FishTrajectory::kFixedTimeStep);
    if (acceleration >
        World11FishTrajectory::kMaximumAcceleration + 2.0e-2f) {
      fail("maximum acceleration was exceeded at t=" +
          std::to_string(time));
    }
    const float turnCosine = std::clamp(
        vectorDot(previous.velocity, current.velocity) /
            std::max(previousSpeed * currentSpeed, 1.0e-6f),
        -1.0f, 1.0f);
    const float turnAngle = std::acos(turnCosine);
    const float maximumTurn = World11FishTrajectory::kMaximumTurnRate *
        static_cast<float>(World11FishTrajectory::kFixedTimeStep);
    if (turnAngle > maximumTurn + 2.0e-3f) {
      fail("maximum turn rate was exceeded at t=" + std::to_string(time) +
          ", angle=" + std::to_string(turnAngle) +
          ", limit=" + std::to_string(maximumTurn));
    }
    previous = current;
  }

  float repeatedWindowDistance = 0.0f;
  for (std::size_t second = 0; second + 120U < seconds.size(); ++second) {
    repeatedWindowDistance += distance(seconds[second], seconds[second + 120U]);
  }
  repeatedWindowDistance /= static_cast<float>(seconds.size() - 120U);
  if (repeatedWindowDistance < 2.0f) {
    fail("ten-minute route resembles a short repeating loop");
  }

  World11FishTrajectory changedSeed(kSeed ^ 0xD1B54A32D192ED03ULL);
  float seedRouteDifference = 0.0f;
  for (int second = 30; second <= 600; second += 30) {
    seedRouteDifference += distance(
        route.sample(static_cast<double>(second)).position,
        changedSeed.sample(static_cast<double>(second)).position);
  }
  if (seedRouteDifference < 50.0f) {
    fail("changing trajectorySeed does not materially change the route");
  }

  World11FishTrajectory phaseCheck(kSeed);
  const World11FishState phaseA = phaseCheck.sample(10.0);
  const World11FishState phaseB = phaseCheck.sample(10.25);
  if (phaseA.tailPhase == phaseB.tailPhase ||
      distance(phaseA.position, phaseB.position) < 0.05f) {
    fail("fish or tail phase did not advance continuously");
  }

  std::cout << "World11 fish trajectory tests passed\n";
  return EXIT_SUCCESS;
}
