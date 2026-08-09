#include "ecs.h"
#include "query.h"
#include "raylib.h"
#include "sqlite3.h"
#include "update_system.h"
#include <stdio.h>

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f

#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

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

static void drawPlayer(Query *render)
{
    if (!query_next(render)) return;

    float px = (float)render->tables[0].data[0];
    float py = (float)render->tables[0].data[1];

    DrawRectangleRec((Rectangle){ px + 4.0f, py + 6.0f, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec((Rectangle){ px, py, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 228, 179, 48, 255 });
    DrawRectangleLinesEx((Rectangle){ px + 1.5f, py + 1.5f, PLAYER_SIZE - 3.0f, PLAYER_SIZE - 3.0f },
                         3.0f, (Color){ 138, 106, 21, 255 });
}

int main(void)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(":memory:", &db,
                        SQLITE_OPEN_MEMORY | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open_v2: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (ecs_setup(db) != 0) {
        sqlite3_close(db);
        return 1;
    }

    static const char *pos_cols[] = { "x", "y" };
    static const char *vel_cols[] = { "x", "y" };
    UpdateSystemConfig cfg = {
        .dim = 2,
        .pos_cols = pos_cols,
        .vel_cols = vel_cols,
        .params = { ACCEL, MAX_SPEED, FRICTION },
        .lo = { 0.0f, 0.0f },
        .hi = { PLATE_W - PLAYER_SIZE, PLATE_H - PLAYER_SIZE },
    };

    UpdateSystem update_system;
    if (update_system_init(&update_system, db, &cfg) != 0) {
        sqlite3_close(db);
        return 1;
    }

    static const char *render_pos_cols[] = { "x", "y" };
    CompSpec render_specs[2] = {
        { "position", 2, render_pos_cols },
        { "square",   0, NULL },
    };
    Query render_query;
    if (query_init(&render_query, db, render_specs, 2) != 0) {
        update_system_destroy(&update_system);
        sqlite3_close(db);
        return 1;
    }

    InitWindow((int)PLATE_W, (int)PLATE_H, "Raylib Square Game");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        float input[2] = {
            (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
              - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)),
            (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
              - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)),
        };

        update_system_run(&update_system, dt, input);

        BeginDrawing();
        drawBaseplate();
        query_reset(&render_query);
        drawPlayer(&render_query);
        DrawText("Move with WASD or arrow keys", 12, (int)PLATE_H - 28, 20,
                 (Color){ 255, 255, 255, 140 });
        EndDrawing();
    }

    CloseWindow();
    query_destroy(&render_query);
    update_system_destroy(&update_system);
    sqlite3_close(db);
    return 0;
}
