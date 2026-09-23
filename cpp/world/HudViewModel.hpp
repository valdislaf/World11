#pragma once

#include <string>

namespace hg::world {

/// <summary>Prepared text lines for the runtime HUD overlay.</summary>
struct HudViewModel {
  std::string speedText;
  std::string localCameraText;
  std::string globalCameraText;
  std::string worldDistanceText;
  std::string worldDistanceUnitsText;

  /// <summary>Reads runtime state and formats all HUD text without OpenGL calls.</summary>
  static HudViewModel create();
};

}  // namespace hg::world
