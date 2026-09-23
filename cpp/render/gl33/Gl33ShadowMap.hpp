#pragma once

#include "render/gl33/Gl33Api.hpp"
#include "render/gl33/Gl33Renderer.hpp"

#include <array>
#include <string>

namespace hg::render::gl33 {

class Gl33ShaderProgram;

/// <summary>Texture unit reserved for the World 11 shadow map in receiving shaders.</summary>
inline constexpr int kWorld11ShadowTextureUnit = 6;

/// <summary>Direction in which World 11 sunlight travels; matches <c>uLightDirection</c>.</summary>
inline constexpr std::array<float, 3> kWorld11LightDirection{-0.32f, -0.88f, -0.36f};

/// <summary>
/// Inserts the shadow uniforms and <c>world11ShadowVisibility(position, normal,
/// lightDirection)</c> after the <c>#version</c> line of a receiving fragment shader.
/// </summary>
std::string withWorld11Shadows(const char* fragmentSource);

/// <summary>
/// Points <c>uShadowMap</c> at the reserved unit and disables sampling, so a
/// receiver built for World 11 stays valid when drawn without a shadow map.
/// </summary>
void initializeWorld11ShadowReceiver(const Gl33ShaderProgram& program);

/// <summary>
/// Directional-light depth map for World 11. The orthographic light frustum
/// follows the camera and is snapped to whole texels to keep edges stable.
/// </summary>
class Gl33ShadowMap final {
public:
  Gl33ShadowMap();
  ~Gl33ShadowMap();
  Gl33ShadowMap(const Gl33ShadowMap&) = delete;
  Gl33ShadowMap& operator=(const Gl33ShadowMap&) = delete;

  /// <summary>Fits the light frustum to the camera, then binds and clears the depth target.</summary>
  void beginDepthPass(const Gl33Camera& camera);
  /// <summary>Restores the default framebuffer and the previous viewport.</summary>
  void endDepthPass();
  /// <returns>Column-major light view matrix for shadow-casting draws.</returns>
  const float* lightView() const;
  /// <returns>Column-major light projection matrix for shadow-casting draws.</returns>
  const float* lightProjection() const;
  /// <summary>
  /// Binds the depth map and uploads sampling uniforms to a receiver built with
  /// <see cref="withWorld11Shadows"/>. The program must be current.
  /// </summary>
  void applyToReceiver(const Gl33ShaderProgram& program) const;
  /// <summary>Releases the reserved texture unit before the next depth pass.</summary>
  void unbind() const;

private:
  void ensureInitialized();

  UInt framebuffer_ = 0;
  UInt depthTexture_ = 0;
  float depthBias_ = 0.0f;
  std::array<Int, 4> previousViewport_{};
  std::array<float, 16> view_{};
  std::array<float, 16> projection_{};
  std::array<float, 16> viewProjection_{};
};

}  // namespace hg::render::gl33
