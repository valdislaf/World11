#pragma once

/// <summary>
/// Plain-C ABI bridge exposing the C++ runtime (<see cref="hg::Engine"/>, worlds, camera and
/// universe state) to non-C++ callers. Function names and signatures are a stable public
/// contract; do not rename or change signatures without a compatibility review.
/// </summary>

#ifdef __cplusplus
extern "C" {
#endif

/* Engine handle API */
/// <returns>Newly allocated engine instance. Ownership transfers to the caller, who must release it via <see cref="hg_engine_destroy"/>. Never returns <c>nullptr</c>.</returns>
void* hg_engine_create(void);
/// <summary>Destroys an engine instance previously returned by <see cref="hg_engine_create"/>.</summary>
/// <param name="engine">Engine handle to destroy. Passing <c>nullptr</c> is a no-op.</param>
void hg_engine_destroy(void* engine);
/// <summary>Advances the engine by one frame (input polling, active world tick, rendering).</summary>
/// <param name="engine">Engine handle from <see cref="hg_engine_create"/>. Must not be <c>nullptr</c>.</param>
/// <param name="dt">Frame delta time in seconds.</param>
void hg_engine_update(void* engine, double dt);
/// <param name="engine">Engine handle from <see cref="hg_engine_create"/>. Must not be <c>nullptr</c>.</param>
/// <returns>Number of frames processed by <see cref="hg_engine_update"/> so far.</returns>
int hg_engine_frame_count(void* engine);
/// <param name="engine">Engine handle from <see cref="hg_engine_create"/>. Must not be <c>nullptr</c>.</param>
/// <returns>Total elapsed engine time in seconds.</returns>
double hg_engine_time_seconds(void* engine);

/* Runtime/world control API */
/// <summary>Generates universe-space anchors for all worlds and resets the global camera to the current world's anchor.</summary>
void hg_universe_init(void);
/// <returns>Total number of configured worlds.</returns>
int hg_world_count(void);
/// <summary>Recomputes the global camera position from the active world's anchor plus a local offset.</summary>
/// <param name="x">Local X offset from the active world's anchor.</param>
/// <param name="y">Local Y offset from the active world's anchor.</param>
/// <param name="z">Local Z offset from the active world's anchor.</param>
void hg_universe_update_global_from_local(double x, double y, double z);
/// <summary>Makes a world active without moving the global camera position.</summary>
/// <param name="world_id">1-based world id; out-of-range values are ignored.</param>
void hg_universe_set_current_world_only(int world_id);
/// <summary>Sets the number of worlds to generate anchors for on the next <see cref="hg_universe_init"/> call.</summary>
/// <param name="world_count">Desired world count; clamped to at least 1.</param>
void hg_runtime_configure_world_count(int world_count);
/// <summary>Resets all per-world runtime flags (e.g. vertical motion) for the given number of worlds.</summary>
/// <param name="world_count">Number of worlds to allocate flags for; clamped to at least 1.</param>
void hg_runtime_configure_world_flags(int world_count);
/// <summary>Switches the active world and moves the global camera to that world's anchor.</summary>
/// <param name="world_id">1-based world id; out-of-range values are ignored.</param>
void hg_runtime_set_world(int world_id);
/// <returns>1-based id of the currently active world.</returns>
int hg_runtime_world(void);
/// <returns>Camera X position in the active world's local coordinates.</returns>
double hg_runtime_camera_x(void);
/// <returns>Camera Y position in the active world's local coordinates.</returns>
double hg_runtime_camera_y(void);
/// <returns>Camera Z position in the active world's local coordinates.</returns>
double hg_runtime_camera_z(void);
/// <returns>Camera front-direction vector components (X/Y/Z), derived from yaw/pitch.</returns>
double hg_runtime_camera_front_x(void);
double hg_runtime_camera_front_y(void);
double hg_runtime_camera_front_z(void);
/// <returns>Camera up-direction vector components (X/Y/Z), derived from yaw/pitch.</returns>
double hg_runtime_camera_up_x(void);
double hg_runtime_camera_up_y(void);
double hg_runtime_camera_up_z(void);
/// <returns>Camera yaw in degrees.</returns>
double hg_runtime_camera_yaw(void);
/// <returns>Camera pitch in degrees, clamped to +/-89.</returns>
double hg_runtime_camera_pitch(void);
/// <returns>Non-zero until the first mouse sample has been recorded via <see cref="hg_runtime_set_camera_mouse_state"/>.</returns>
int hg_runtime_camera_first_mouse(void);
/// <returns>Last recorded cursor position (X/Y) used to compute mouse-look deltas.</returns>
double hg_runtime_camera_last_mouse_x(void);
double hg_runtime_camera_last_mouse_y(void);
/// <summary>Records the last-seen cursor position, used to compute mouse-look deltas.</summary>
/// <param name="first_mouse">Non-zero to mark that no delta should be computed from this sample.</param>
/// <param name="last_x">Cursor X position to store.</param>
/// <param name="last_y">Cursor Y position to store.</param>
void hg_runtime_set_camera_mouse_state(int first_mouse, double last_x, double last_y);
/// <summary>Sets camera yaw/pitch and the corresponding front/up basis vectors.</summary>
/// <param name="yaw">Yaw in degrees.</param>
/// <param name="pitch">Pitch in degrees.</param>
/// <param name="front_x">Front vector X component.</param>
/// <param name="front_y">Front vector Y component.</param>
/// <param name="front_z">Front vector Z component.</param>
/// <param name="up_x">Up vector X component.</param>
/// <param name="up_y">Up vector Y component.</param>
/// <param name="up_z">Up vector Z component.</param>
void hg_runtime_set_camera_orientation(double yaw,
                                       double pitch,
                                       double front_x,
                                       double front_y,
                                       double front_z,
                                       double up_x,
                                       double up_y,
                                       double up_z);
/// <returns>Last camera speed reported for the HUD, in world units per second.</returns>
double hg_runtime_camera_speed(void);
/// <summary>Sets the camera speed value reported to the HUD.</summary>
/// <param name="speed">Speed in world units per second.</param>
void hg_runtime_set_camera_speed(double speed);
/// <returns>Camera X position in universe (global) coordinates.</returns>
double hg_runtime_global_camera_x(void);
/// <returns>Camera Y position in universe (global) coordinates.</returns>
double hg_runtime_global_camera_y(void);
/// <returns>Camera Z position in universe (global) coordinates.</returns>
double hg_runtime_global_camera_z(void);
/// <returns>Universe-space anchor X of the active world.</returns>
double hg_runtime_world_anchor_x(void);
/// <returns>Universe-space anchor Y of the active world.</returns>
double hg_runtime_world_anchor_y(void);
/// <returns>Universe-space anchor Z of the active world.</returns>
double hg_runtime_world_anchor_z(void);
/// <param name="world_id">1-based world id.</param>
/// <returns>Universe-space anchor X of the given world, or 0 if out of range.</returns>
double hg_runtime_world_anchor_x_at(int world_id);
/// <param name="world_id">1-based world id.</param>
/// <returns>Universe-space anchor Y of the given world, or 0 if out of range.</returns>
double hg_runtime_world_anchor_y_at(int world_id);
/// <param name="world_id">1-based world id.</param>
/// <returns>Universe-space anchor Z of the given world, or 0 if out of range.</returns>
double hg_runtime_world_anchor_z_at(int world_id);
/// <summary>Overrides the camera's local position and re-syncs its universe-space (global) position from it.</summary>
/// <param name="x">Local X position in the active world.</param>
/// <param name="y">Local Y position in the active world.</param>
/// <param name="z">Local Z position in the active world.</param>
void hg_runtime_set_camera_position(double x, double y, double z);
/// <summary>Makes a world active while preserving the current universe-space (global) camera position.</summary>
/// <param name="world_id">1-based world id; out-of-range values are ignored.</param>
void hg_runtime_set_world_preserve_global(int world_id);
/// <summary>Enables or disables physics-based (C++) vertical motion handling for a world.</summary>
/// <param name="world_id">1-based world id; values less than 1 are ignored.</param>
/// <param name="enabled">Non-zero to enable C++-driven vertical motion for this world.</param>
void hg_runtime_set_cpp_vertical_motion(int world_id, int enabled);
/// <param name="world_id">1-based world id.</param>
/// <returns>Non-zero if C++ vertical motion is enabled for the given world.</returns>
int hg_runtime_is_cpp_vertical_motion_enabled(int world_id);

#ifdef __cplusplus
}
#endif
