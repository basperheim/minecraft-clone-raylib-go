// hello_cube.c
// Build: see commands below
#include "raylib.h"
#include "raymath.h"
#include <math.h>

int main(void) {
    const int screenW = 1280;
    const int screenH = 720;
    InitWindow(screenW, screenH, "Hello Cube - Raylib FPS (walk & jump)");
    SetTargetFPS(60);

    // Camera (we'll control position + yaw/pitch)
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 4.0f }; // eye pos
    camera.target   = (Vector3){ 0.0f, 1.6f, 0.0f }; // look-at
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Controls / physics
    float yaw = 3.14159f;                 // facing -Z initially
    float pitch = -0.15f;
    float mouseSens = 0.0025f;
    float speed = 6.0f;
    float sprintMult = 1.8f;
    float eyeHeight = 1.7f;

    float velY = 0.0f;
    const float gravity = -18.0f;
    const float jumpSpeed = 6.5f;

    // Hide cursor and capture mouse for mouselook
    bool cursorLocked = true;
    DisableCursor();

    // Simple world ground (y = 0)
    const float groundY = 0.0f;

    // Cube at origin for reference
    Vector3 cubePos = { 0.0f, 0.5f, 0.0f };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Toggle cursor lock
        if (IsKeyPressed(KEY_TAB)) {
            cursorLocked = !cursorLocked;
            if (cursorLocked) DisableCursor(); else EnableCursor();
        }

        // Mouse look
        if (cursorLocked) {
            Vector2 d = GetMouseDelta();
            yaw   -= d.x * mouseSens;
            pitch -= d.y * mouseSens;
            const float limit = PI/2.2f;
            if (pitch > limit) pitch = limit;
            if (pitch < -limit) pitch = -limit;
        }

        // Build forward/right vectors from yaw/pitch (standard FPS math)
        float cp = cosf(pitch);
        float sp = sinf(pitch);
        float sy = sinf(yaw);
        float cy = cosf(yaw);

        Vector3 forward = { cp*sy, sp, -cp*cy }; // note: z negative forward
        // flatten for ground movement
        Vector3 forwardGround = { forward.x, 0.0f, forward.z };
        float fglen = Vector3Length(forwardGround);
        if (fglen > 0.0001f) forwardGround = Vector3Scale(forwardGround, 1.0f/fglen);

        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

        // Movement input
        Vector3 move = { 0, 0, 0 };
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forwardGround);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forwardGround);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);

        // normalize move
        float mlen = Vector3Length(move);
        if (mlen > 0.0001f) move = Vector3Scale(move, 1.0f / mlen);

        float curSpeed = speed;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) curSpeed *= sprintMult;

        // Apply horizontal movement
        camera.position = Vector3Add(camera.position, Vector3Scale(move, curSpeed * dt));

        // Gravity + ground collision
        velY += gravity * dt;
        camera.position.y += velY * dt;

        float minY = groundY + eyeHeight;
        bool onGround = false;
        if (camera.position.y <= minY) {
            camera.position.y = minY;
            velY = 0;
            onGround = true;
        }

        // Jump
        if (onGround && IsKeyPressed(KEY_SPACE)) {
            velY = jumpSpeed;
            onGround = false;
        }

        // Update camera target from yaw/pitch (look at camera.position + forward)
        camera.target = Vector3Add(camera.position, forward);

        // --- drawing ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        // ground grid
        DrawGrid(16, 1.0f);

        // reference cube
        DrawCube(cubePos, 1.0f, 1.0f, 1.0f, MAROON);
        DrawCubeWires(cubePos, 1.0f, 1.0f, 1.0f, BLACK);

        EndMode3D();

        DrawText("WASD to move, SPACE jump, SHIFT sprint, TAB toggle cursor", 10, 10, 14, DARKGRAY);
        DrawFPS(10, 30);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
