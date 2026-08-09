#include "raylib.h"
#include <math.h>

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f
#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

static float px = 380.0f;
static float py = 230.0f;
static float vx = 0.0f;
static float vy = 0.0f;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void update(float dt, float inx, float iny)
{
    float len = sqrtf(inx * inx + iny * iny);
    if (len > 1.0f) {
        inx /= len;
        iny /= len;
    }

    vx += inx * ACCEL * dt;
    vy += iny * ACCEL * dt;

    float decay = 1.0f / (1.0f + FRICTION * dt);
    vx *= decay;
    vy *= decay;

    vx = clampf(vx, -MAX_SPEED, MAX_SPEED);
    vy = clampf(vy, -MAX_SPEED, MAX_SPEED);

    px += vx * dt;
    py += vy * dt;

    if (px < 0.0f) { px = 0.0f; vx = 0.0f; }
    if (px > PLATE_W - PLAYER_SIZE) { px = PLATE_W - PLAYER_SIZE; vx = 0.0f; }
    if (py < 0.0f) { py = 0.0f; vy = 0.0f; }
    if (py > PLATE_H - PLAYER_SIZE) { py = PLATE_H - PLAYER_SIZE; vy = 0.0f; }
}

static void drawBaseplate(void)
{
    ClearBackground((Color){ 43, 110, 70, 255 });

    float tile = 50.0f;
    for (int y = 0; y < PLATE_H / tile; y++) {
        for (int x = 0; x < PLATE_W / tile; x++) {
            if ((x + y) % 2 == 0) {
                DrawRectangleRec((Rectangle){ x * tile, y * tile, tile, tile },
                                 (Color){ 255, 255, 255, 13 });
            }
        }
    }

    DrawRectangleLinesEx((Rectangle){ 2.0f, 2.0f, PLATE_W - 4.0f, PLATE_H - 4.0f },
                         4.0f, (Color){ 0, 0, 0, 89 });
}

static void drawPlayer(void)
{
    DrawRectangleRec((Rectangle){ px + 4.0f, py + 6.0f, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec((Rectangle){ px, py, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 228, 179, 48, 255 });
    DrawRectangleLinesEx((Rectangle){ px + 1.5f, py + 1.5f, PLAYER_SIZE - 3.0f, PLAYER_SIZE - 3.0f },
                         3.0f, (Color){ 138, 106, 21, 255 });
}

int main(void)
{
    InitWindow((int)PLATE_W, (int)PLATE_H, "Raylib Square Game");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        float inx = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                  - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
        float iny = (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
                  - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W));

        update(dt, inx, iny);

        BeginDrawing();
        drawBaseplate();
        drawPlayer();
        DrawText("Move with WASD or arrow keys", 12, (int)PLATE_H - 28, 20,
                 (Color){ 255, 255, 255, 140 });
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
