#include "engine.h"
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>

// color helper for non-textured fallback
static Color tileColorForIndex(int tile) {
    switch (tile % 8) {
        case 0: return (Color){  80, 170,  80, 255 }; // grass green
        case 1: return (Color){ 255, 180,  60, 255 }; // flowers-ish / warm
        case 2: return (Color){ 139, 105,  80, 255 }; // dirt brown
        case 3: return (Color){ 230, 220, 170, 255 }; // sand beige
        case 4: return (Color){ 150, 150, 150, 255 }; // stone gray
        case 5: return (Color){  70, 130, 200, 255 }; // water blue
        case 6: return (Color){ 150, 110,  70, 255 }; // wood brown
        case 7: return (Color){ 245, 250, 255, 255 }; // snow
        default: return (Color){255,  64, 255, 255};   // magenta = debug
    }
}

typedef struct {
    int sx, sy, sz;       // world dims
    uint16_t* v;          // [sx*sy*sz] block ids
} World;

typedef struct {
    Texture2D* tiles;     // per-tile textures (sliced from atlas)
    int tile_count;
    int tile_px, cols, rows;
    Texture2D atlas_tex;  // optional: whole atlas
    Image     atlas_img;  // kept until slicing done
    bool      atlas_loaded;
} Atlas;

typedef struct {
    uint16_t tile_of_block[256]; // block_id -> tile_index (0..tile_count-1), 0xFFFF=undefined
} BlockDefs;

struct Engine {
    // window/render
    int screen_w, screen_h;
    Camera3D cam;
    float yaw, pitch;
    bool  cursor_locked;

    // assets/world
    Atlas atlas;
    BlockDefs defs;
    World world;

    // Inverted (Minecraft) mouse
    bool invert_mouse_x;
    bool invert_mouse_y;

    // movement
    float move_speed, sprint_mult, eye_height;
    float velY, gravity, jump_speed;
};

static inline int idx3D(const World* w, int x,int y,int z) {
    return x + y*w->sx + z*w->sx*w->sy;
}

Engine* engine_create(int width, int height, const char* title, int target_fps) {
    Engine* e = (Engine*)calloc(1, sizeof(Engine));
    e->screen_w = width; e->screen_h = height;

    InitWindow(width, height, title ? title : "mini3d");
    SetTargetFPS(target_fps > 0 ? target_fps : 60);

    e->cam.position   = (Vector3){ 0, 2, 4 };
    e->cam.target     = (Vector3){ 0, 1.6f, 0 };
    e->cam.up         = (Vector3){ 0, 1, 0 };
    e->cam.fovy       = 60.0f;
    e->cam.projection = CAMERA_PERSPECTIVE;

    e->yaw = PI;   // look -Z
    e->pitch = -0.15f;

    e->cursor_locked = true;
    DisableCursor();

    e->move_speed = 6.0f;
    e->sprint_mult = 1.8f;
    e->eye_height = 1.7f;
    e->velY = 0.0f; e->gravity = -18.0f; e->jump_speed = 6.5f;

    for (int i=0;i<256;i++) e->defs.tile_of_block[i] = 0xFFFF;

    return e;
}

void engine_destroy(Engine* e) {
    if (!e) return;
    // free world
    free(e->world.v);

    // unload tile textures
    if (e->atlas.tiles) {
        for (int i=0;i<e->atlas.tile_count;i++) {
            if (e->atlas.tiles[i].id) UnloadTexture(e->atlas.tiles[i]);
        }
        free(e->atlas.tiles);
    }
    if (e->atlas.atlas_tex.id) UnloadTexture(e->atlas.atlas_tex);
    if (e->atlas.atlas_loaded) UnloadImage(e->atlas.atlas_img);

    CloseWindow();
    free(e);
}

bool engine_load_atlas(Engine* e, const char* png_path, int tile_px, int cols, int rows) {
    if (!e) return false;
    e->atlas.atlas_img = LoadImage(png_path);
    if (!e->atlas.atlas_img.data) return false;
    e->atlas.atlas_loaded = true;

    e->atlas.atlas_tex = LoadTextureFromImage(e->atlas.atlas_img);
    if (!e->atlas.atlas_tex.id) return false;

    // Safer: only call SetTextureFilter if the symbol exists in this build
    #ifdef FILTER_BILINEAR
        SetTextureFilter(e->atlas.atlas_tex, FILTER_BILINEAR);
    #endif

    e->atlas.tile_px = tile_px; e->atlas.cols = cols; e->atlas.rows = rows;
    e->atlas.tile_count = cols*rows;
    e->atlas.tiles = (Texture2D*)calloc(e->atlas.tile_count, sizeof(Texture2D));

    // Slice atlas into per-tile textures (simplest path with raylib)
    for (int i=0;i<e->atlas.tile_count;i++) {
        int col = i % cols, row = i / cols;
        Rectangle rec = (Rectangle){ (float)(col*tile_px), (float)(row*tile_px), (float)tile_px, (float)tile_px };
        Image sub = ImageFromImage(e->atlas.atlas_img, rec);
        e->atlas.tiles[i] = LoadTextureFromImage(sub);
        UnloadImage(sub);
    }
    return true;
}

bool engine_define_block_tile(Engine* e, uint16_t block_id, int tile_index) {
    if (!e || tile_index < 0 || tile_index >= e->atlas.tile_count) return false;
    e->defs.tile_of_block[block_id] = (uint16_t)tile_index;
    return true;
}

bool engine_create_world(Engine* e, int sx, int sy, int sz) {
    if (!e || sx<=0 || sy<=0 || sz<=0) return false;
    free(e->world.v);
    e->world.sx = sx; e->world.sy = sy; e->world.sz = sz;
    size_t N = (size_t)sx*sy*sz;
    e->world.v = (uint16_t*)calloc(N, sizeof(uint16_t));
    return e->world.v != NULL;
}

void engine_clear_world(Engine* e, uint16_t id) {
    if (!e || !e->world.v) return;
    size_t N = (size_t)e->world.sx*e->world.sy*e->world.sz;
    for (size_t i=0;i<N;i++) e->world.v[i] = id;
}

bool engine_set_block(Engine* e, int x,int y,int z, uint16_t block_id) {
    if (!e || !e->world.v) return false;
    if (x<0||y<0||z<0||x>=e->world.sx||y>=e->world.sy||z>=e->world.sz) return false;
    e->world.v[idx3D(&e->world,x,y,z)] = block_id;
    return true;
}

uint16_t engine_get_block(Engine* e, int x,int y,int z) {
    if (!e || !e->world.v) return 0;
    if (x<0||y<0||z<0||x>=e->world.sx||y>=e->world.sy||z>=e->world.sz) return 0;
    return e->world.v[idx3D(&e->world,x,y,z)];
}

void engine_fill_box(Engine* e, int x0,int y0,int z0, int x1,int y1,int z1, uint16_t id) {
    if (!e || !e->world.v) return;
    if (x0>x1){int t=x0;x0=x1;x1=t;} if (y0>y1){int t=y0;y0=y1;y1=t;} if (z0>z1){int t=z0;z0=z1;z1=t;}
    x0 = x0<0?0:x0; y0=y0<0?0:y0; z0=z0<0?0:z0;
    x1 = x1>=e->world.sx?e->world.sx-1:x1;
    y1 = y1>=e->world.sy?e->world.sy-1:y1;
    z1 = z1>=e->world.sz?e->world.sz-1:z1;
    for (int z=z0; z<=z1; z++)
    for (int y=y0; y<=y1; y++) {
        int base = y*e->world.sx + z*e->world.sx*e->world.sy;
        for (int x=x0; x<=x1; x++) e->world.v[base + x] = id;
    }
}

void engine_set_camera_pose(Engine* e, float x,float y,float z, float yaw,float pitch) {
    if (!e) return;
    e->cam.position = (Vector3){x,y,z};
    e->yaw = yaw; e->pitch = pitch;
}

void engine_get_camera_pose(Engine* e, float* x,float* y,float* z, float* yaw,float* pitch) {
    if (!e) return;
    if (x) *x = e->cam.position.x;
    if (y) *y = e->cam.position.y;
    if (z) *z = e->cam.position.z;
    if (yaw) *yaw = e->yaw;
    if (pitch) *pitch = e->pitch;
}

static void process_input(Engine* e, float dt) {
    // Toggle cursor lock
    if (IsKeyPressed(KEY_TAB)) {
        e->cursor_locked = !e->cursor_locked;
        if (e->cursor_locked) DisableCursor(); else EnableCursor();
    }

    // Mouse look
    if (e->cursor_locked) {
        Vector2 d = GetMouseDelta();
        const float sens = 0.0025f;

        float mx = e->invert_mouse_x ? -d.x : d.x;
        float my = e->invert_mouse_y ? -d.y : d.y;   // invert Y when true (Minecraft-style)

        e->yaw   += mx * sens;    // horizontal: mouse-right -> look-right
        e->pitch += my * sens;    // vertical: sign controlled by invert_mouse_y

        float limit = PI/2.2f;
        if (e->pitch > limit) e->pitch = limit;
        if (e->pitch < -limit) e->pitch = -limit;
    }

    // Build forward/right
    float cp = cosf(e->pitch), sp = sinf(e->pitch);
    float sy = sinf(e->yaw),   cy = cosf(e->yaw);
    Vector3 forward = (Vector3){ cp*sy, sp, -cp*cy };

    Vector3 fg = (Vector3){ forward.x, 0, forward.z };
    float len = Vector3Length(fg);
    if (len > 1e-4f) fg = Vector3Scale(fg, 1.0f/len);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, e->cam.up));

    float speed = e->move_speed;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) speed *= e->sprint_mult;

    Vector3 move = (Vector3){0,0,0};
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, fg);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, fg);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    float m = Vector3Length(move);
    if (m > 1e-4f) move = Vector3Scale(move, 1.0f/m);

    e->cam.position = Vector3Add(e->cam.position, Vector3Scale(move, speed*dt));

    // gravity + simple ground (y=0)
    e->velY += e->gravity*dt;
    e->cam.position.y += e->velY*dt;
    float minY = 0.0f + e->eye_height;
    bool onGround = false;
    if (e->cam.position.y <= minY) {
        e->cam.position.y = minY; e->velY = 0; onGround = true;
    }
    if (onGround && IsKeyPressed(KEY_SPACE)) e->velY = e->jump_speed;

    e->cam.target = Vector3Add(e->cam.position, forward);
}

static void draw_world(Engine* e) {
    BeginMode3D(e->cam);
    DrawGrid(32, 1.0f);

    if (!e->world.v || !e->atlas.tiles) {
        EndMode3D();
        return;
    }

    // VERY NAIVE: draw every nonzero block.
    // Start small (e.g. 64^3). Optimize later with chunking/meshing.
    // VERY NAIVE: draw every nonzero block.
    for (int z=0; z<e->world.sz; z++)
    for (int y=0; y<e->world.sy; y++)
    for (int x=0; x<e->world.sx; x++) {
        uint16_t id = e->world.v[idx3D(&e->world,x,y,z)];
        if (!id) continue;
        uint16_t tile = e->defs.tile_of_block[id];
        if (tile == 0xFFFF || tile >= e->atlas.tile_count) continue;
        // If we had per-tile textures and DrawCubeTexture is available you can swap back later.
        // For broad compatibility use colored cubes for now:
        Vector3 pos = (Vector3){ (float)x, (float)y, (float)z };
        Color c = tileColorForIndex((int)tile);
        DrawCube(pos, 1.0f, 1.0f, 1.0f, c);
        DrawCubeWires(pos, 1.0f, 1.0f, 1.0f, Fade(BLACK, 0.2f));
    }

    EndMode3D();
}

bool engine_tick(Engine* e, float dt) {
    if (!e) return false;
    if (WindowShouldClose()) return false;

    process_input(e, dt);

    BeginDrawing();
    ClearBackground(RAYWHITE);
    draw_world(e);
    DrawText("WASD move | SPACE jump | SHIFT sprint | TAB cursor", 10, 10, 14, DARKGRAY);
    DrawFPS(10, 30);
    EndDrawing();

    return true;
}
