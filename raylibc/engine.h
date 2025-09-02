#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct Engine Engine;   // opaque

// Create/destroy
Engine* engine_create(int width, int height, const char* title, int target_fps);
void    engine_destroy(Engine* e);

// Camera control (optional; engine also handles WASD/mouse)
void engine_set_camera_pose(Engine* e, float x, float y, float z, float yaw, float pitch);
void engine_get_camera_pose(Engine* e, float* x, float* y, float* z, float* yaw, float* pitch);

// Load a tile atlas and slice it into per-tile textures (for simplicity).
// Example: 512x512 atlas with 64x64 tiles => tile_px=64, cols=8, rows=8
bool engine_load_atlas(Engine* e, const char* png_path, int tile_px, int cols, int rows);

// Define which tile index a block-id uses (same texture on all faces for now).
// You can extend later to (top/side/bottom) if you want.
bool engine_define_block_tile(Engine* e, uint16_t block_id, int tile_index);

// World allocation (simple dense 3D array; start small: 64x64x64)
bool engine_create_world(Engine* e, int sx, int sy, int sz);
void engine_clear_world(Engine* e, uint16_t block_id); // fill entire world with id (0 = empty)

// Set/Read blocks
bool engine_set_block(Engine* e, int x, int y, int z, uint16_t block_id);
uint16_t engine_get_block(Engine* e, int x, int y, int z);

// Main step: processes input, draws a frame, returns false to request quit
bool engine_tick(Engine* e, float dt);

// Convenience: build a flat terrain column (helper)
void engine_fill_box(Engine* e, int x0,int y0,int z0, int x1,int y1,int z1, uint16_t block_id);

#ifdef __cplusplus
}
#endif
