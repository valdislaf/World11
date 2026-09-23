#pragma once

#include "render/gl33/Gl33ShaderProgram.hpp"
#include "world/World11WaterSurface.hpp"

namespace hg::render::gl33 {

/// <summary>Shared optical parameters for World 11 opaque objects.</summary>
inline void setWorld11Environment(Gl33ShaderProgram& program) {
  program.setVec3("uFogColor", 0.10f, 0.32f, 0.40f);
  program.setFloat("uWaterSurfaceY", world::kWorld11WaterBaseY);
  program.setFloat("uAbsorptionDensity", 0.008f);
  program.setFloat("uDepthAbsorption", 0.0017f);
}

}  // namespace hg::render::gl33
