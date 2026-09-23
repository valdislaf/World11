#include "world/HudViewModel.hpp"

#include "hg_runtime_api.h"

#include <cmath>
#include <cstdio>

namespace hg::world {

namespace {

constexpr double kAuInMeters = 1.495978707e11;
constexpr double kLyInMeters = 9.4607304725808e15;

double anchorDistanceMeters() {
  const double ax = hg_runtime_world_anchor_x();
  const double ay = hg_runtime_world_anchor_y();
  const double az = hg_runtime_world_anchor_z();
  const double maximum = std::fmax(std::fmax(std::fabs(ax), std::fabs(ay)), std::fabs(az));
  if (maximum <= 0.0) {
    return 0.0;
  }
  return maximum * std::sqrt((ax / maximum) * (ax / maximum) +
                             (ay / maximum) * (ay / maximum) +
                             (az / maximum) * (az / maximum));
}

}  // namespace

HudViewModel HudViewModel::create() {
  HudViewModel model;
  char text[192] = {};

  std::snprintf(text, sizeof(text), "Speed: %7.2f m/s", hg_runtime_camera_speed());
  model.speedText = text;
  std::snprintf(text, sizeof(text), "Cam(local): %8.2f%8.2f%8.2f m",
                hg_runtime_camera_x(), hg_runtime_camera_y(), hg_runtime_camera_z());
  model.localCameraText = text;
  std::snprintf(text, sizeof(text), "World %d global: % .4E % .4E % .4E m",
                hg_runtime_world(), hg_runtime_global_camera_x(),
                hg_runtime_global_camera_y(), hg_runtime_global_camera_z());
  model.globalCameraText = text;

  const double distanceMeters = anchorDistanceMeters();
  std::snprintf(text, sizeof(text), "World dist: % .4E m", distanceMeters);
  model.worldDistanceText = text;
  std::snprintf(text, sizeof(text), "          (% .3E AU, % .3E ly)",
                distanceMeters / kAuInMeters, distanceMeters / kLyInMeters);
  model.worldDistanceUnitsText = text;
  return model;
}

}  // namespace hg::world
