#pragma once

namespace hg {

/// <summary>Per-world boolean flags, currently used to opt a world into physics-based (C++) vertical motion.</summary>
class RuntimeWorldFlags {
public:
  /// <summary>Resets all per-world flags to false for the given number of worlds.</summary>
  /// <param name="world_count">Number of worlds to allocate flags for; clamped to at least 1.</param>
  static void configure(int world_count);
  /// <summary>Enables or disables C++ vertical motion handling for a world, growing storage if needed.</summary>
  /// <param name="world_id">1-based world id; values less than 1 are ignored.</param>
  /// <param name="enabled">True to enable C++-driven vertical motion for this world.</param>
  static void setCppVerticalMotion(int world_id, bool enabled);
  /// <param name="world_id">1-based world id.</param>
  /// <returns>True if C++ vertical motion is enabled for the given world; false if out of range or not configured.</returns>
  static bool isCppVerticalMotion(int world_id);
};

}  // namespace hg
