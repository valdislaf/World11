#pragma once

#include <array>

namespace hg {

/// <summary>
/// Tracks the fixed universe-space anchor of each world and the camera's current
/// global position, so the runtime can place worlds far apart while worlds themselves
/// still work in small local coordinates.
/// </summary>
class RuntimeUniverse {
public:
  /// <summary>Sets the number of worlds to generate anchors for on the next <see cref="init"/> call, discarding any existing anchors.</summary>
  /// <param name="world_count">Desired world count; clamped to at least 1.</param>
  static void configureWorldCount(int world_count);
  /// <summary>
  /// (Re)generates world anchors in universe space if not already generated, and resets the
  /// global camera position to the current world's anchor. Safe to call multiple times.
  /// </summary>
  static void init();
  /// <returns>The 1-based id of the currently active world. Initializes the universe on first use.</returns>
  static int currentWorld();
  /// <returns>Total number of configured/generated worlds. Initializes the universe on first use if needed.</returns>
  static int worldCount();
  /// <summary>Makes <paramref name="world_id"/> the active world and moves the global camera to that world's anchor.</summary>
  /// <param name="world_id">1-based world id; out-of-range values are ignored.</param>
  static void switchWorld(int world_id);
  /// <summary>Makes <paramref name="world_id"/> the active world without touching the global camera position.</summary>
  /// <param name="world_id">1-based world id; out-of-range values are ignored.</param>
  static void setCurrentWorldOnly(int world_id);
  /// <summary>Recomputes the global camera position from the active world's anchor plus a local offset.</summary>
  /// <param name="x">Local X offset from the active world's anchor.</param>
  /// <param name="y">Local Y offset from the active world's anchor.</param>
  /// <param name="z">Local Z offset from the active world's anchor.</param>
  static void updateGlobalFromLocal(double x, double y, double z);
  /// <returns>Current camera position in universe (global) coordinates.</returns>
  static std::array<double, 3> globalCamera();
  /// <param name="world_id">1-based world id.</param>
  /// <returns>Universe-space anchor of the given world, or the origin if <paramref name="world_id"/> is out of range.</returns>
  static std::array<double, 3> worldAnchor(int world_id);
};

}  // namespace hg
