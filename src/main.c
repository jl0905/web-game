#include "ecs.h"
#include "plugin_economy.h"
#include "plugin_fishing.h"
#include "plugin_game.h"
#include "raylib.h"
#include "sqlite3.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

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

    // The game simulates and draws in a fixed logical resolution (PLATE_W x
    // PLATE_H) and is scaled to whatever window/monitor size the player picks,
    // letterboxed to preserve aspect. The window must exist before the plugins
    // load their textures and fonts.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Emberholm - Fishing Draft");
    SetWindowMinSize(400, 250);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture((int)PLATE_W, (int)PLATE_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // Plugins register their component definitions and systems; main() only
    // drives their lifecycle and builds the scene entities below from the
    // exposed component definitions.
    GamePlugin game = { 0 };
    FishingPlugin fishing = { 0 };
    EconomyPlugin economy = { 0 };
    ui_init();
    if (game_plugin_init(&game, db) != 0) goto fail;
    if (fishing_plugin_init(&fishing, db, &game) != 0) goto fail;
    if (economy_plugin_init(&economy, db, &game, &fishing) != 0) goto fail;

    // --- Player (game plugin) + wallet (economy plugin) ----------------------
    game.player_id = ecs_entity_create(db);
    if (game.player_id < 1) goto fail;
    ecs_component_set(db, &game.pos, game.player_id, (float[]){ 380.0f, 230.0f });
    ecs_component_set(db, &game.vel, game.player_id, (float[]){ 0.0f, 0.0f });
    ecs_component_set(db, &game.square, game.player_id, NULL);
    ecs_component_set(db, &game.color, game.player_id, (float[]){ 228, 179, 48, 255 });
    ecs_component_set(db, &economy.wallet, game.player_id, (float[]){ 0 });

    // World boundary (defines the plate and its walls). Its rectangle mirrors
    // the game plugin's plate constants, which also set the physics bounds; it
    // is NOT a push-out solid, or the player (who lives inside it) would be
    // shoved off-screen.
    game.boundary_id = ecs_entity_create(db);
    if (game.boundary_id < 1) goto fail;
    ecs_component_set(db, &game.rect, game.boundary_id,
                      (float[]){ 0.0f, 0.0f, game.plate_w, game.plate_h });

    // Lake (fishing plugin)
    fishing.lake_id = ecs_entity_create(db);
    if (fishing.lake_id < 1) goto fail;
    ecs_component_set(db, &game.rect, fishing.lake_id, (float[]){ 560.0f, 90.0f, 220.0f, 330.0f });
    ecs_component_set(db, &game.solid, fishing.lake_id, NULL);
    ecs_component_set(db, &game.color, fishing.lake_id, (float[]){ 38, 92, 150, 255 });
    ecs_text_set(db, "name", "label", fishing.lake_id, "LAKE");

    // Shop (fishing plugin)
    fishing.shop_id = ecs_entity_create(db);
    if (fishing.shop_id < 1) goto fail;
    ecs_component_set(db, &game.rect, fishing.shop_id, (float[]){ 30.0f, 30.0f, 120.0f, 90.0f });
    ecs_component_set(db, &game.solid, fishing.shop_id, NULL);
    ecs_component_set(db, &game.color, fishing.shop_id, (float[]){ 120, 80, 48, 255 });
    ecs_text_set(db, "name", "label", fishing.shop_id, "FISH SHOP");

    // Fishing state machine (state + minigame + hooked fish)
    fishing.state_id = ecs_entity_create(db);
    if (fishing.state_id < 1) goto fail;
    ecs_component_set(db, &fishing.game_comp, fishing.state_id, (float[]){ STATE_WALK, 0.0f, 0.0f });
    ecs_component_set(db, &fishing.minigame, fishing.state_id,
                      (float[]){ 0.5f, 0.5f, 0.0f, 0.3f, 0.0f, 0.4f });
    ecs_component_set(db, &fishing.fish, fishing.state_id, (float[]){ 0, 0.0f, 0 });

    // Popup entity (the "fish above the head" icon)
    fishing.popup_id = ecs_entity_create(db);
    if (fishing.popup_id < 1) goto fail;
    ecs_component_set(db, &fishing.popup, fishing.popup_id, (float[]){ 255, 255, 255, 255, 0 });
    ecs_text_set(db, "popup_text", "text", fishing.popup_id, "");
    ecs_component_set(db, &fishing.fish, fishing.popup_id, (float[]){ 0, 0.0f, 0 });

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        // Manual scheduling until a real scheduler exists: the game plugin
        // drives the player controller while walking, and the fishing plugin
        // owns the rest of the state machine.
        if (fishing_plugin_state(&fishing) == STATE_WALK)
            game_plugin_update(&game, dt, &fishing, &economy);
        else
            fishing_plugin_update(&fishing, dt);

        BeginTextureMode(target);
        fishing_plugin_draw_world(&fishing);
        game_plugin_draw(&game);
        fishing_plugin_draw_hud(&fishing);
        economy_plugin_draw(&economy);
        EndTextureMode();

        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        float scale = fminf(sw / PLATE_W, sh / PLATE_H);
        float outW = PLATE_W * scale;
        float outH = PLATE_H * scale;

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture,
                       (Rectangle){ 0, 0, PLATE_W, -PLATE_H }, // flip: RT is y-inverted
                       (Rectangle){ (sw - outW) / 2.0f, (sh - outH) / 2.0f, outW, outH },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    economy_plugin_destroy(&economy);
    fishing_plugin_destroy(&fishing);
    game_plugin_destroy(&game);
    ui_destroy();
    UnloadRenderTexture(target);
    CloseWindow();
    sqlite3_close(db);
    return 0;

fail:
    economy_plugin_destroy(&economy);
    fishing_plugin_destroy(&fishing);
    game_plugin_destroy(&game);
    ui_destroy();
    UnloadRenderTexture(target);
    CloseWindow();
    sqlite3_close(db);
    return 1;
}
