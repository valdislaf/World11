#pragma once

/// <summary>Tuning constants shared by keyboard, mouse and gamepad camera controllers.</summary>
namespace hg::camera_tuning {

/// <summary>Base camera movement speed, in world units per second, before sprint/gamepad modifiers.</summary>
constexpr double kMoveSpeedBase = 4.52;
/// <summary>Multiplier applied to <see cref="kMoveSpeedBase"/> while sprinting.</summary>
constexpr double kSprintMultiplier = 20.0;
/// <summary>Multiplier applied to gamepad-driven movement speed.</summary>
constexpr double kGamepadBoostFactor = 5.0;

}  // namespace hg::camera_tuning
