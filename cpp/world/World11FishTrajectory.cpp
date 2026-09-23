#include "world/World11FishTrajectory.hpp"

#include "world/World11Seabed.hpp"
#include "world/World11WaterSurface.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hg::world {

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kMinimumSpeed = 0.72f;

World11FishVec3 add(
    const World11FishVec3& lhs, const World11FishVec3& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

World11FishVec3 subtract(
    const World11FishVec3& lhs, const World11FishVec3& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

World11FishVec3 multiply(const World11FishVec3& value, float factor) {
  return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(const World11FishVec3& lhs, const World11FishVec3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

World11FishVec3 cross(
    const World11FishVec3& lhs, const World11FishVec3& rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

float length(const World11FishVec3& value) {
  return std::sqrt(dot(value, value));
}

World11FishVec3 normalize(
    const World11FishVec3& value,
    const World11FishVec3& fallback = {1.0f, 0.0f, 0.0f}) {
  const float valueLength = length(value);
  if (valueLength < 1.0e-6f) {
    return fallback;
  }
  return multiply(value, 1.0f / valueLength);
}

World11FishVec3 clampLength(
    const World11FishVec3& value, float maximumLength) {
  const float valueLength = length(value);
  if (valueLength <= maximumLength || valueLength < 1.0e-6f) {
    return value;
  }
  return multiply(value, maximumLength / valueLength);
}

World11FishVec3 rotateTowards(
    const World11FishVec3& from, const World11FishVec3& to,
    float maximumAngle) {
  const World11FishVec3 fromUnit = normalize(from);
  const World11FishVec3 toUnit = normalize(to, fromUnit);
  const float cosine = std::clamp(dot(fromUnit, toUnit), -1.0f, 1.0f);
  const float angle = std::acos(cosine);
  if (angle <= maximumAngle || angle < 1.0e-5f) {
    return toUnit;
  }

  const float fraction = maximumAngle / angle;
  const float sine = std::sin(angle);
  if (std::abs(sine) > 1.0e-4f) {
    const float fromWeight = std::sin((1.0f - fraction) * angle) / sine;
    const float toWeight = std::sin(fraction * angle) / sine;
    return normalize(add(
        multiply(fromUnit, fromWeight), multiply(toUnit, toWeight)));
  }

  World11FishVec3 axis = cross(fromUnit, {0.0f, 1.0f, 0.0f});
  if (length(axis) < 1.0e-4f) {
    axis = cross(fromUnit, {1.0f, 0.0f, 0.0f});
  }
  axis = normalize(axis);
  return normalize(add(
      multiply(fromUnit, std::cos(maximumAngle)),
      multiply(axis, std::sin(maximumAngle))));
}

float smoothstep(float edge0, float edge1, float value) {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

World11FishTrajectory::World11FishTrajectory(
    std::uint64_t trajectorySeed, World11FishVec3 initialPosition,
    World11FishMovementVolume movementVolume)
    : trajectorySeed_(trajectorySeed),
      initialPosition_(initialPosition),
      movementVolume_(movementVolume) {
  movementVolume_.horizontalRadius = std::clamp(
      movementVolume_.horizontalRadius, 12.0f, 18.0f);
  movementVolume_.surfaceClearance = std::max(
      movementVolume_.surfaceClearance, 0.5f);
  movementVolume_.seabedClearance = std::max(
      movementVolume_.seabedClearance, 0.4f);
  movementVolume_.fishHalfHeight = std::max(
      movementVolume_.fishHalfHeight, 0.0f);
  reset();
}

World11FishState World11FishTrajectory::sample(double simulationTime) {
  if (!std::isfinite(simulationTime) || simulationTime < 0.0) {
    simulationTime = 0.0;
  }
  const std::uint64_t targetTicks = static_cast<std::uint64_t>(
      std::floor(simulationTime / kFixedTimeStep + 1.0e-9));
  if (targetTicks < simulatedTicks_) {
    reset();
  }

  while (simulatedTicks_ < targetTicks) {
    advance(fixedState_, kFixedTimeStep);
    ++simulatedTicks_;
    fixedState_.time = static_cast<double>(simulatedTicks_) * kFixedTimeStep;
  }

  SimulationState sampled = fixedState_;
  const double fixedTime = static_cast<double>(targetTicks) * kFixedTimeStep;
  const double remainder = simulationTime - fixedTime;
  if (remainder > 1.0e-9) {
    advance(sampled, std::min(remainder, kFixedTimeStep));
  }
  return sampled.fish;
}

void World11FishTrajectory::reset() {
  fixedState_ = {};
  simulatedTicks_ = 0;
  fixedState_.fish.position = initialPosition_;
  configureEvent(fixedState_, 0);
  fixedState_.fish.velocity = multiply(
      fixedState_.fish.desiredDirection, fixedState_.targetSpeed);
  enforceBounds(fixedState_);
}

std::uint64_t World11FishTrajectory::trajectorySeed() const {
  return trajectorySeed_;
}

const World11FishVec3& World11FishTrajectory::initialPosition() const {
  return initialPosition_;
}

const World11FishMovementVolume&
World11FishTrajectory::movementVolume() const {
  return movementVolume_;
}

std::uint64_t World11FishTrajectory::stableHash(
    std::uint64_t seed, std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  value ^= value >> 31U;
  seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  seed = (seed ^ (seed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  seed = (seed ^ (seed >> 27U)) * 0x94D049BB133111EBULL;
  return seed ^ (seed >> 31U);
}

float World11FishTrajectory::hashUnit(std::uint64_t value) {
  return static_cast<float>(value >> 40U) * (1.0f / 16777216.0f);
}

void World11FishTrajectory::configureEvent(
    SimulationState& state, std::uint64_t eventIndex) const {
  const std::uint64_t eventHash = stableHash(trajectorySeed_, eventIndex);
  const float azimuth = kTwoPi * hashUnit(stableHash(
      eventHash, 0x415A494D555448ULL));
  const float vertical = -0.24f + 0.48f * hashUnit(stableHash(
      eventHash, 0x564552544943414CULL));
  const float horizontal = std::sqrt(std::max(0.0f, 1.0f - vertical * vertical));
  state.fish.desiredDirection = {
      std::cos(azimuth) * horizontal,
      vertical,
      std::sin(azimuth) * horizontal,
  };
  state.targetSpeed = 1.05f + 1.05f * hashUnit(stableHash(
      eventHash, 0x5350454544ULL));
  const float duration = 2.0f + 4.0f * hashUnit(stableHash(
      eventHash, 0x4455524154494F4EULL));
  state.fish.trajectoryEventIndex = eventIndex;
  state.nextEventTime = state.time + static_cast<double>(duration);
}

void World11FishTrajectory::advance(
    SimulationState& state, double deltaTime) const {
  double remaining = std::clamp(deltaTime, 0.0, 0.05);
  while (remaining > 1.0e-10) {
    if (state.time + 1.0e-10 >= state.nextEventTime) {
      configureEvent(state, state.fish.trajectoryEventIndex + 1U);
    }
    const double untilEvent = std::max(0.0, state.nextEventTime - state.time);
    const double segment = std::min(remaining, untilEvent);
    if (segment <= 1.0e-10) {
      configureEvent(state, state.fish.trajectoryEventIndex + 1U);
      continue;
    }
    integrate(state, static_cast<float>(segment));
    state.time += segment;
    enforceBounds(state);
    remaining -= segment;
  }
}

void World11FishTrajectory::integrate(
    SimulationState& state, float deltaTime) const {
  const float speed = length(state.fish.velocity);
  const World11FishVec3 currentDirection = normalize(
      state.fish.velocity, state.fish.desiredDirection);

  const float horizontalOffsetX =
      state.fish.position.x - movementVolume_.center.x;
  const float horizontalOffsetZ =
      state.fish.position.z - movementVolume_.center.z;
  const float radialDistance = std::sqrt(
      horizontalOffsetX * horizontalOffsetX +
      horizontalOffsetZ * horizontalOffsetZ);
  const float softRadius = movementVolume_.horizontalRadius * 0.55f;
  World11FishVec3 steeringDirection = state.fish.desiredDirection;
  World11FishVec3 inward{0.0f, 0.0f, 0.0f};
  float boundaryStrength = 0.0f;
  if (radialDistance > softRadius) {
    boundaryStrength = smoothstep(
        softRadius, movementVolume_.horizontalRadius * 0.90f,
        radialDistance);
    inward = normalize(
        {-horizontalOffsetX, 0.0f, -horizontalOffsetZ});
    steeringDirection = normalize(add(
        multiply(state.fish.desiredDirection, 1.0f - boundaryStrength),
        multiply(inward, boundaryStrength * 1.65f)), inward);
  }

  const World11FishVec3 desiredVelocity = multiply(
      steeringDirection, state.targetSpeed);
  World11FishVec3 acceleration = multiply(
      subtract(desiredVelocity, state.fish.velocity), 0.82f);
  if (radialDistance > softRadius) {
    acceleration = add(acceleration, multiply(
        inward, kMaximumAcceleration * (0.20f + 0.80f * boundaryStrength)));
  }

  const float surfaceLimit = world11WaterSurfaceHeight(
      state.fish.position.x, state.fish.position.z,
      static_cast<float>(state.time)) -
      movementVolume_.surfaceClearance - movementVolume_.fishHalfHeight;
  const float seabedLimit = world11SeabedHeight(
      state.fish.position.x, state.fish.position.z) +
      movementVolume_.seabedClearance + movementVolume_.fishHalfHeight;
  constexpr float kVerticalSteeringBand = 1.8f;
  if (state.fish.position.y > surfaceLimit - kVerticalSteeringBand) {
    const float strength = smoothstep(
        surfaceLimit - kVerticalSteeringBand, surfaceLimit,
        state.fish.position.y);
    acceleration.y -= kMaximumAcceleration * (0.30f + 0.70f * strength);
  }
  if (state.fish.position.y < seabedLimit + kVerticalSteeringBand) {
    const float strength = 1.0f - smoothstep(
        seabedLimit, seabedLimit + kVerticalSteeringBand,
        state.fish.position.y);
    acceleration.y += kMaximumAcceleration * (0.30f + 0.70f * strength);
  }

  const float time = static_cast<float>(state.time + 0.5 * deltaTime);
  const float phaseVerticalA = kTwoPi * hashUnit(stableHash(
      trajectorySeed_, 0x4E4F495345564131ULL));
  const float phaseVerticalB = kTwoPi * hashUnit(stableHash(
      trajectorySeed_, 0x4E4F495345564232ULL));
  const float phaseSideA = kTwoPi * hashUnit(stableHash(
      trajectorySeed_, 0x4E4F495345534131ULL));
  const float phaseSideB = kTwoPi * hashUnit(stableHash(
      trajectorySeed_, 0x4E4F495345534232ULL));
  const float verticalNoise = 0.055f * (
      std::sin(time * 0.73f + phaseVerticalA) +
      0.47f * std::sin(time * 1.31f + phaseVerticalB));
  const World11FishVec3 horizontalForward = normalize(
      {currentDirection.x, 0.0f, currentDirection.z});
  const World11FishVec3 side = normalize(cross(
      {0.0f, 1.0f, 0.0f}, horizontalForward), {0.0f, 0.0f, 1.0f});
  const float sideNoise = 0.070f * (
      std::sin(time * 0.61f + phaseSideA) +
      0.41f * std::sin(time * 1.17f + phaseSideB));
  acceleration.y += verticalNoise;
  acceleration = add(acceleration, multiply(side, sideNoise));
  acceleration = clampLength(acceleration, kMaximumAcceleration);

  World11FishVec3 candidateVelocity = add(
      state.fish.velocity, multiply(acceleration, deltaTime));
  float candidateSpeed = std::clamp(
      length(candidateVelocity), kMinimumSpeed, kMaximumSpeed);
  const World11FishVec3 candidateDirection = rotateTowards(
      currentDirection, candidateVelocity,
      kMaximumTurnRate * deltaTime);
  candidateVelocity = multiply(candidateDirection, candidateSpeed);
  state.fish.velocity = candidateVelocity;
  state.fish.position = add(
      state.fish.position, multiply(candidateVelocity, deltaTime));

  const float tailFrequency = 0.78f + 0.76f * candidateSpeed;
  state.fish.tailPhase = std::fmod(
      state.fish.tailPhase + kTwoPi * tailFrequency * deltaTime,
      kTwoPi);
}

void World11FishTrajectory::enforceBounds(SimulationState& state) const {
  const float offsetX = state.fish.position.x - movementVolume_.center.x;
  const float offsetZ = state.fish.position.z - movementVolume_.center.z;
  const float radialDistance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
  if (radialDistance > movementVolume_.horizontalRadius) {
    const float normalX = offsetX / radialDistance;
    const float normalZ = offsetZ / radialDistance;
    state.fish.position.x = movementVolume_.center.x +
        normalX * movementVolume_.horizontalRadius;
    state.fish.position.z = movementVolume_.center.z +
        normalZ * movementVolume_.horizontalRadius;
    const float outwardSpeed =
        state.fish.velocity.x * normalX + state.fish.velocity.z * normalZ;
    if (outwardSpeed > 0.0f) {
      state.fish.velocity.x -= normalX * (outwardSpeed + 0.12f);
      state.fish.velocity.z -= normalZ * (outwardSpeed + 0.12f);
    }
  }

  const float surfaceLimit = world11WaterSurfaceHeight(
      state.fish.position.x, state.fish.position.z,
      static_cast<float>(state.time)) -
      movementVolume_.surfaceClearance - movementVolume_.fishHalfHeight;
  const float seabedLimit = world11SeabedHeight(
      state.fish.position.x, state.fish.position.z) +
      movementVolume_.seabedClearance + movementVolume_.fishHalfHeight;
  const float lower = std::min(seabedLimit, surfaceLimit);
  const float upper = std::max(seabedLimit, surfaceLimit);
  if (state.fish.position.y < lower) {
    state.fish.position.y = lower;
    state.fish.velocity.y = std::max(state.fish.velocity.y, 0.12f);
  } else if (state.fish.position.y > upper) {
    state.fish.position.y = upper;
    state.fish.velocity.y = std::min(state.fish.velocity.y, -0.12f);
  }
  state.fish.velocity = clampLength(
      state.fish.velocity, kMaximumSpeed);
}

}  // namespace hg::world
