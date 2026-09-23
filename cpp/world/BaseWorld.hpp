#pragma once

#include "hg_interfaces.hpp"
#include "world/PortalController.hpp"
#include "world/VerticalMotionController.hpp"
#include "world/collision/CollisionResolver.hpp"
#include "world/collision/CollisionWorld.hpp"

#include <cstddef>

namespace hg::world {

/// <summary>
/// Template-method base class for future C++ worlds.
/// </summary>
/// <remarks>
/// Provides the common frame order used by the current C++ worlds without requiring them
/// to inherit from this class yet. Concrete worlds can override protected hooks for
/// loading, gravity flags, terrain contact, portal handling and drawing.
/// </remarks>
class BaseWorld : public IWorld {
public:
  /// <summary>Constructs a base world with empty collision and motion state.</summary>
  BaseWorld();
  /// <summary>Constructs a base world with shared portal switching and custom portal drawing.</summary>
  /// <param name="portals">Pointer to immutable portal data; not owned and must outlive this world.</param>
  /// <param name="portalCount">Number of elements in <paramref name="portals"/>.</param>
  BaseWorld(const Portal* portals, std::size_t portalCount);
  /// <summary>Destroys the base world through the world interface.</summary>
  ~BaseWorld() override = default;

  /// <summary>
  /// Advances one frame using the shared C++ world order: cooldown, active-world check,
  /// begin frame, physics, portals, scene drawing, portal drawing, end frame and engine update.
  /// </summary>
  /// <param name="engine">Engine facade with frame/time/camera state.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Inactive-world ticks still run begin/end frame and reset vertical motion, matching existing worlds.</remarks>
  void tick(Engine& engine, double dt) override;
  /// <summary>Registers this world's colliders into <c>collisionWorld_</c>.</summary>
  virtual void initColliders() = 0;
  /// <returns>This world's numeric id, as reported to the runtime.</returns>
  int getWorldId() const override = 0;

protected:
  /// <summary>Hook invoked before portal cooldown and active-world detection.</summary>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Use for per-frame state that existing worlds update even while inactive.</remarks>
  virtual void beforeWorldCheck(Engine& engine, double dt);
  /// <summary>Controls whether portal cooldown is decremented before active-world detection.</summary>
  /// <returns>True when cooldown should tick before checking the active world.</returns>
  /// <remarks>Concrete worlds can override this when their frame order requires it.</remarks>
  virtual bool shouldTickPortalCooldownBeforeWorldCheck() const;
  /// <summary>Runs the inactive-world frame path.</summary>
  /// <param name="engine">Engine facade advanced at the end of the inactive frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Default implementation calls RuntimeBridge begin/end frame, resets vertical motion and updates the engine.</remarks>
  virtual void tickInactiveWorld(Engine& engine, double dt);
  /// <summary>Hook invoked after active-world detection and before <c>RuntimeBridge::beginFrame</c>.</summary>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Use for lazy resources that existing worlds load before beginning the frame, such as skyboxes.</remarks>
  virtual void beforeBeginFrame(Engine& engine, double dt);
  /// <summary>Hook invoked immediately after <c>RuntimeBridge::beginFrame</c>.</summary>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Default implementation updates the per-world vertical physics flag from RuntimeBridge.</remarks>
  virtual void afterBeginFrame(Engine& engine, double dt);
  /// <summary>Resolves camera motion, door blocking and collision, then updates camera position if needed.</summary>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <returns>Resolved camera position after physics and door blocking.</returns>
  virtual collision::Vec3 doPhysicsTick(Engine& engine, double dt);
  /// <summary>Returns the ground contact height used by vertical motion.</summary>
  /// <param name="x">World-space X coordinate to sample.</param>
  /// <param name="z">World-space Z coordinate to sample.</param>
  /// <returns>Ground height at the given X/Z coordinates.</returns>
  virtual double groundContactAt(double x, double z) const;
  /// <summary>Returns the camera collision sphere radius for this world.</summary>
  /// <returns>Camera collision radius in world units.</returns>
  virtual double cameraRadius() const;
  /// <summary>Hook for portal proximity/switch handling after physics and before scene drawing.</summary>
  /// <param name="resolved">Resolved camera position after physics.</param>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <returns>True if the portal switch consumed the rest of the frame.</returns>
  virtual bool handlePortals(const collision::Vec3& resolved, Engine& engine, double dt);
  /// <summary>Draws the world's scene. Must be implemented by every concrete world.</summary>
  void drawScene() const override = 0;
  /// <summary>Draws portal markers after scene drawing.</summary>
  /// <remarks>Default implementation is a no-op for worlds without visible portal markers.</remarks>
  virtual void drawPortals() const;
  /// <summary>Hook invoked immediately before <c>RuntimeBridge::endFrame</c> on active-world paths.</summary>
  /// <param name="engine">Engine facade for the current frame.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  /// <remarks>Use for frame-scope OpenGL cleanup that must run before every active end-frame path.</remarks>
  virtual void beforeEndFrame(Engine& engine, double dt);

  /// <summary>Collider storage shared with <see cref="collisionResolver_"/>; populated via <see cref="initColliders"/>.</summary>
  collision::CollisionWorld collisionWorld_;
  collision::CollisionResolver collisionResolver_;
  VerticalMotionController verticalMotion_;
  collision::Vec3 prevCameraPos_{0.0, 0.0, 0.0};
  bool prevCameraValid_ = false;
  int portalCooldownFrames_ = 0;
  bool enableVerticalPhysics_ = false;
  float doorOpenAmount_ = 0.0f;

  /// <summary>Updates <c>doorOpenAmount_</c> based on camera proximity. Default implementation does nothing; override in worlds with doors.</summary>
  /// <param name="camera_pos">Current camera position before physics resolution.</param>
  /// <param name="dt">Frame delta time in seconds.</param>
  virtual void updateDoor(const collision::Vec3& camera_pos, double dt);
  /// <summary>Clamps a physics-resolved position against a closed door. Default implementation is a pass-through; override in worlds with doors.</summary>
  /// <param name="camera_pos">Camera position before physics resolution.</param>
  /// <param name="resolved">Camera position after physics/collision resolution.</param>
  /// <returns>Position to actually apply to the camera.</returns>
  virtual collision::Vec3 applyDoorBlock(const collision::Vec3& camera_pos, const collision::Vec3& resolved) const;

  /// <summary>Resets per-world motion state after portal switches or inactive ticks.</summary>
  /// <remarks>Matches the reset performed by the current C++ worlds when leaving or entering through a portal.</remarks>
  void resetMotionState();

  /// <summary>Default floor height for worlds using flat ground contact.</summary>
  static constexpr float kFloorY = -1.0f;
  /// <summary>Default ceiling height for worlds with corridor-style ceilings.</summary>
  static constexpr float kCeilingY = 2.45f;
  /// <summary>Default camera collision sphere radius.</summary>
  static constexpr double kCameraRadius = 0.35;

private:
  const Portal* portals_ = nullptr;
  std::size_t portalCount_ = 0;
};

}  // namespace hg::world
