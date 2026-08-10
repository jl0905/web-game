#include "plugin_fishing.h"

#include "ecs.h"
#include "plugin_game.h"
#include "raylib.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Minigame: vertical track; keep the catch bar over the darting fish.
#define TRACK_H 260.0f
#define BAR_H 70.0f

// --- Fish data ---------------------------------------------------------------

static const FishSpecies SPECIES[FISH_SPECIES_COUNT] = {
    { "Pond Minnow",   8.0f,   4.0f,  12.0f, 40, { 176, 196, 222, 255 }, NULL },
    { "Green Perch",  15.0f,  10.0f,  30.0f, 30, { 107, 142,  35, 255 }, NULL },
    { "Amber Carp",   30.0f,  20.0f,  60.0f, 18, { 218, 165,  32, 255 }, NULL },
    { "Blue Pike",    60.0f,  35.0f,  90.0f,  9, {  70, 130, 180, 255 }, NULL },
    { "Squid",        80.0f,  25.0f, 100.0f,  6, { 205, 170, 160, 255 }, "assets/squid.png" },
    { "King Sturgeon", 150.0f, 60.0f, 150.0f, 3, { 147, 112, 219, 255 }, NULL },
};

static const char *fish_cols[] = { "species", "length", "shiny" };
static const char *game_cols[] = { "state", "timer", "bite_after" };
static const char *minigame_cols[] = { "fish_pos", "fish_target", "fish_vel",
                                       "bar_pos", "bar_vel", "catch_progress" };
static const char *popup_cols[] = { "r", "g", "b", "a", "show_fish" };

float fishing_plugin_fish_price(const Fish *f)
{
    const FishSpecies *s = &SPECIES[f->species];
    float avg = (s->minLen + s->maxLen) * 0.5f;
    float price = s->basePrice * (f->length / avg);
    if (f->shiny) price *= 10.0f;
    return price;
}

static float rand01(void)
{
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

static Fish rollFish(void)
{
    int total = 0;
    for (int i = 0; i < FISH_SPECIES_COUNT; i++) total += SPECIES[i].weight;
    int pick = GetRandomValue(1, total);
    int idx = 0;
    for (int i = 0; i < FISH_SPECIES_COUNT; i++) {
        pick -= SPECIES[i].weight;
        if (pick <= 0) { idx = i; break; }
    }

    const FishSpecies *s = &SPECIES[idx];
    // Heavy right tail: long fish stay rare (see DESIGN.md fishing tech notes)
    float t = powf(rand01(), 2.5f);
    Fish f;
    f.species = idx;
    f.length = s->minLen + (s->maxLen - s->minLen) * t;
    f.shiny = GetRandomValue(1, SHINY_ODDS) == 1;
    return f;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// --- Component accessors -----------------------------------------------------

static void readGame(const FishingPlugin *p, float out[3])
{
    ecs_component_get(p->db, &p->game_comp, p->state_id, out);
}

static void writeGame(const FishingPlugin *p, float state, float timer, float biteAfter)
{
    ecs_component_set(p->db, &p->game_comp, p->state_id, (float[]){ state, timer, biteAfter });
}

static void readMinigame(const FishingPlugin *p, float out[6])
{
    ecs_component_get(p->db, &p->minigame, p->state_id, out);
}

static void writeMinigame(const FishingPlugin *p, const float in[6])
{
    ecs_component_set(p->db, &p->minigame, p->state_id, in);
}

static void readHooked(const FishingPlugin *p, Fish *f)
{
    float d[3];
    ecs_component_get(p->db, &p->fish, p->state_id, d);
    f->species = (int)d[0];
    f->length = d[1];
    f->shiny = d[2] != 0.0f;
}

static void writeHooked(const FishingPlugin *p, const Fish *f)
{
    ecs_component_set(p->db, &p->fish, p->state_id,
                      (float[]){ (float)f->species, f->length, f->shiny ? 1.0f : 0.0f });
}

static void readPopupDisplay(const FishingPlugin *p, Color *c, bool *showFish)
{
    float d[5];
    ecs_component_get(p->db, &p->popup, p->popup_id, d);
    c->r = (unsigned char)d[0];
    c->g = (unsigned char)d[1];
    c->b = (unsigned char)d[2];
    c->a = (unsigned char)d[3];
    *showFish = d[4] != 0.0f;
}

static void writePopupDisplay(const FishingPlugin *p, Color c, bool showFish)
{
    ecs_component_set(p->db, &p->popup, p->popup_id,
                      (float[]){ c.r, c.g, c.b, c.a, showFish ? 1.0f : 0.0f });
}

static void readPopupFish(const FishingPlugin *p, Fish *f)
{
    float d[3];
    ecs_component_get(p->db, &p->fish, p->popup_id, d);
    f->species = (int)d[0];
    f->length = d[1];
    f->shiny = d[2] != 0.0f;
}

static void writePopupFish(const FishingPlugin *p, const Fish *f)
{
    ecs_component_set(p->db, &p->fish, p->popup_id,
                      (float[]){ (float)f->species, f->length, f->shiny ? 1.0f : 0.0f });
}

static void readPopupText(const FishingPlugin *p, char *out, size_t n)
{
    ecs_text_get(p->db, "popup_text", "text", p->popup_id, out, n);
}

// --- Public queries / actions ------------------------------------------------

int fishing_plugin_state(const FishingPlugin *p)
{
    float g[3];
    if (ecs_component_get(p->db, &p->game_comp, p->state_id, g) != 0)
        return STATE_WALK;
    return (int)g[0];
}

int fishing_plugin_inventory_count(FishingPlugin *p)
{
    int n = 0;
    query_reset(&p->inventory_query);
    while (query_next(&p->inventory_query)) n++;
    return n;
}

static bool playerNear(Rectangle r, float px, float py, float size)
{
    Rectangle player = { px, py, size, size };
    Rectangle grown = { r.x - INTERACT_RANGE, r.y - INTERACT_RANGE,
                        r.width + 2 * INTERACT_RANGE, r.height + 2 * INTERACT_RANGE };
    return CheckCollisionRecs(player, grown);
}

int fishing_plugin_near_lake(const FishingPlugin *p, float px, float py, float size)
{
    return playerNear(game_plugin_rect(p->game, p->lake_id), px, py, size);
}

int fishing_plugin_near_shop(const FishingPlugin *p, float px, float py, float size)
{
    return playerNear(game_plugin_rect(p->game, p->shop_id), px, py, size);
}

int fishing_plugin_try_cast(FishingPlugin *p, float px, float py, float size)
{
    if (!fishing_plugin_near_lake(p, px, py, size)) return 0;
    writeGame(p, (float)STATE_WAITING, 0.0f, 1.0f + rand01() * 3.0f);
    return 1;
}

void fishing_plugin_show_popup(FishingPlugin *p, const char *text, Color c, const Fish *fish)
{
    writeGame(p, (float)STATE_POPUP, 0.0f, 0.0f);
    ecs_text_set(p->db, "popup_text", "text", p->popup_id, text);
    writePopupDisplay(p, c, fish != NULL);
    if (fish) writePopupFish(p, fish);
}

static void startMinigame(FishingPlugin *p)
{
    Fish h = rollFish();
    writeHooked(p, &h);
    writeGame(p, (float)STATE_MINIGAME, 0.0f, 0.0f);
    float m[6] = { 0.5f, 0.5f, 0.0f, 0.3f, 0.0f, 0.4f };
    writeMinigame(p, m);
}

// --- Update ------------------------------------------------------------------

void fishing_plugin_update(FishingPlugin *p, float dt)
{
    float g[3];
    readGame(p, g);
    int state = (int)g[0];
    g[1] += dt;

    if (state == STATE_WAITING) {
        if (IsKeyPressed(KEY_E)) { writeGame(p, (float)STATE_WALK, 0.0f, 0.0f); return; } // reel in early
        if (g[1] >= g[2]) { writeGame(p, (float)STATE_BITE, 0.0f, 0.0f); return; }
        writeGame(p, (float)state, g[1], g[2]);
        return;
    }

    if (state == STATE_BITE) {
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) { startMinigame(p); return; }
        if (g[1] > 0.7f) {
            fishing_plugin_show_popup(p, "It got away...", LIGHTGRAY, NULL);
        } else {
            writeGame(p, (float)state, g[1], g[2]);
        }
        return;
    }

    if (state == STATE_MINIGAME) {
        float m[6];
        readMinigame(p, m);
        Fish h;
        readHooked(p, &h);

        // Fish darts toward a wandering target; harder species dart faster
        float difficulty = 0.6f + 0.4f * (float)h.species; // 0.6 .. 2.2
        if (rand01() < dt * (0.8f + 0.5f * difficulty)) m[1] = rand01();
        float accel = (m[1] - m[0]) * (3.0f + 2.0f * difficulty);
        m[2] += accel * dt;
        m[2] *= 1.0f / (1.0f + 2.0f * dt);
        m[0] = clampf(m[0] + m[2] * dt, 0.0f, 1.0f);

        // Catch bar: hold SPACE to rise, gravity pulls down (Stardew-style)
        float barSpan = BAR_H / TRACK_H;
        if (IsKeyDown(KEY_SPACE)) m[4] += 3.2f * dt;
        else m[4] -= 3.2f * dt;
        m[4] = clampf(m[4], -1.4f, 1.4f);
        m[3] += m[4] * dt;
        if (m[3] < 0.0f) { m[3] = 0.0f; m[4] = 0.0f; }
        if (m[3] > 1.0f - barSpan) { m[3] = 1.0f - barSpan; m[4] = 0.0f; }

        bool overlap = m[0] >= m[3] && m[0] <= m[3] + barSpan;
        m[5] += (overlap ? 0.35f : -0.25f) * dt;

        writeMinigame(p, m);

        if (m[5] >= 1.0f) {
            if (fishing_plugin_inventory_count(p) >= MAX_INVENTORY) {
                fishing_plugin_show_popup(p, "Caught it - but your bag is full!", ORANGE, NULL);
            } else {
                // The catch becomes its own entity in the bag.
                int64_t caught = ecs_entity_create(p->db);
                ecs_component_set(p->db, &p->fish, caught,
                                  (float[]){ (float)h.species, h.length, h.shiny ? 1.0f : 0.0f });
                ecs_component_set(p->db, &p->inventory, caught, NULL);
                char buf[128];
                snprintf(buf, sizeof(buf), "Caught a %s%s! %.0f cm - worth %d coins",
                         h.shiny ? "SHINY " : "", SPECIES[h.species].name,
                         h.length, (int)fishing_plugin_fish_price(&h));
                fishing_plugin_show_popup(p, buf, h.shiny ? GOLD : SKYBLUE, &h);
            }
        } else if (m[5] <= 0.0f) {
            fishing_plugin_show_popup(p, "The fish escaped!", LIGHTGRAY, NULL);
        }
        return;
    }

    if (state == STATE_POPUP) {
        if (g[1] > 0.6f &&
            (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || g[1] > 3.0f)) {
            writeGame(p, (float)STATE_WALK, 0.0f, 0.0f);
        } else {
            writeGame(p, (float)state, g[1], g[2]);
        }
        return;
    }
}

// --- Drawing -----------------------------------------------------------------

static Texture2D catchTexture(const FishingPlugin *p, int species)
{
    if (p->speciesTex[species].id != 0) return p->speciesTex[species];
    return p->fishTex;
}

void fishing_plugin_draw_world(FishingPlugin *p)
{
    ClearBackground((Color){ 43, 110, 70, 255 });

    float tile = 50.0f;
    for (int y = 0; y < p->game->plate_h / tile; y++) {
        for (int x = 0; x < p->game->plate_w / tile; x++) {
            if ((x + y) % 2 == 0) {
                DrawRectangleRec((Rectangle){ x * tile, y * tile, tile, tile },
                                 (Color){ 255, 255, 255, 13 });
            }
        }
    }

    // Lake
    Rectangle lake = game_plugin_rect(p->game, p->lake_id);
    DrawRectangleRounded(lake, 0.25f, 8, game_plugin_color(p->game, p->lake_id));
    double tnow = GetTime();
    for (int i = 0; i < 4; i++) {
        float wy = lake.y + 40.0f + 70.0f * i + 6.0f * sinf((float)tnow * 1.5f + i * 1.7f);
        DrawLineEx((Vector2){ lake.x + 20, wy }, (Vector2){ lake.x + lake.width - 20, wy },
                   2.0f, (Color){ 255, 255, 255, 40 });
    }

    // Shop
    Rectangle shop = game_plugin_rect(p->game, p->shop_id);
    DrawRectangleRec(shop, game_plugin_color(p->game, p->shop_id));
    DrawRectangleRec((Rectangle){ shop.x, shop.y, shop.width, 22 }, (Color){ 165, 42, 42, 255 });
    char label[64];
    if (ecs_text_get(p->db, "name", "label", p->shop_id, label, sizeof(label)) == 0) {
        ui_text(label, shop.x + 14, shop.y + 2, 18, RAYWHITE);
    }
    DrawRectangleLinesEx(shop, 3.0f, (Color){ 60, 40, 24, 255 });

    // World boundary
    Rectangle bound = game_plugin_rect(p->game, p->game->boundary_id);
    DrawRectangleLinesEx(bound, 4.0f, (Color){ 0, 0, 0, 89 });
}

static void drawMinigame(const FishingPlugin *p)
{
    float m[6];
    readMinigame(p, m);
    Fish h;
    readHooked(p, &h);

    float trackX = p->game->plate_w - 70.0f;
    float trackY = (p->game->plate_h - TRACK_H) / 2.0f;

    DrawRectangleRec((Rectangle){ trackX - 8, trackY - 8, 40, TRACK_H + 16 },
                     (Color){ 20, 30, 40, 230 });
    // Catch bar
    float barSpan = BAR_H / TRACK_H;
    float barY = trackY + TRACK_H * (1.0f - m[3] - barSpan);
    DrawRectangleRec((Rectangle){ trackX - 4, barY, 32, BAR_H }, (Color){ 80, 200, 120, 200 });
    // Fish
    float fy = trackY + TRACK_H * (1.0f - m[0]);
    DrawCircle((int)(trackX + 12), (int)fy, 8.0f,
               h.shiny ? GOLD : SPECIES[h.species].color);
    // Progress bar
    DrawRectangleRec((Rectangle){ trackX + 36, trackY, 10, TRACK_H }, (Color){ 20, 30, 40, 230 });
    DrawRectangleRec((Rectangle){ trackX + 36, trackY + TRACK_H * (1.0f - m[5]),
                                  10, TRACK_H * m[5] },
                     (Color){ 240, 200, 60, 255 });
    ui_text("HOLD SPACE", trackX - 40, trackY + TRACK_H + 14, 16, RAYWHITE);
}

void fishing_plugin_draw_hud(FishingPlugin *p)
{
    int state = fishing_plugin_state(p);
    GamePlugin *game = p->game;

    // Fishing rod + line while fishing
    if (state == STATE_WAITING || state == STATE_BITE || state == STATE_MINIGAME) {
        Rectangle lake = game_plugin_rect(game, p->lake_id);
        Vector2 hand = { game->px + game->player_size, game->py + 8 };
        Vector2 tip = { hand.x + 26, hand.y - 22 };
        Vector2 bobber = { clampf(game->px + game->player_size + 60, lake.x + 15, lake.x + lake.width - 15),
                           clampf(game->py + 20, lake.y + 15, lake.y + lake.height - 15) };
        DrawLineEx(hand, tip, 3.0f, (Color){ 90, 60, 30, 255 });
        DrawLineEx(tip, bobber, 1.0f, (Color){ 230, 230, 230, 200 });
        DrawCircleV(bobber, 5.0f, state == STATE_BITE ? RED : (Color){ 240, 80, 80, 255 });
        if (state == STATE_BITE) {
            ui_text("!", bobber.x - 4, bobber.y - 34, 30, YELLOW);
        }
    }

    if (state == STATE_MINIGAME) drawMinigame(p);

    if (state == STATE_WALK) {
        float px = game->px, py = game->py, size = game->player_size;
        if (fishing_plugin_near_lake(p, px, py, size)) {
            ui_text("[E] Cast line", px - 20, py - 26, 18, RAYWHITE);
        } else if (fishing_plugin_near_shop(p, px, py, size)) {
            ui_text("[E] Sell fish", px - 20, py - 26, 18, RAYWHITE);
        }
    } else if (state == STATE_WAITING) {
        ui_text("Waiting for a bite... [E] reel in", game->px - 60, game->py - 26, 18, RAYWHITE);
    } else if (state == STATE_BITE) {
        ui_text("[E] HOOK IT!", game->px - 20, game->py - 26, 20, YELLOW);
    }

    if (state == STATE_POPUP) {
        char text[128];
        readPopupText(p, text, sizeof(text));
        Color c;
        bool showFish;
        readPopupDisplay(p, &c, &showFish);

        int w = (int)ui_measure(text, 22);
        DrawRectangle((int)(game->plate_w / 2) - w / 2 - 16, 190, w + 32, 46,
                      (Color){ 20, 30, 40, 230 });
        ui_text(text, game->plate_w / 2.0f - w / 2.0f, 200, 22, c);

        // Hold the catch overhead, Stardew-style. The popup entity owns the
        // fish being displayed; shinies get a gold tint.
        if (showFish) {
            Fish pf;
            readPopupFish(p, &pf);
            Texture2D tex = catchTexture(p, pf.species);
            if (tex.id != 0) {
                float s = 64.0f / (float)tex.height; // ~64 px tall on screen
                float fw = tex.width * s, fh = tex.height * s;
                float bob = 4.0f * sinf((float)GetTime() * 4.0f);
                DrawTexturePro(tex,
                               (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                               (Rectangle){ game->px + game->player_size / 2.0f - fw / 2.0f,
                                            game->py - fh - 12.0f + bob, fw, fh },
                               (Vector2){ 0, 0 }, 0.0f,
                               pf.shiny ? GOLD : WHITE);
            }
        }
    }
}

// --- Lifecycle ---------------------------------------------------------------

int fishing_plugin_init(FishingPlugin *p, sqlite3 *db, GamePlugin *game)
{
    p->db = db;
    p->game = game;

    p->fish = (CompSpec){ "fish", 3, fish_cols };
    p->inventory = (CompSpec){ "inventory", 0, NULL };
    p->game_comp = (CompSpec){ "game", 3, game_cols };
    p->minigame = (CompSpec){ "minigame", 6, minigame_cols };
    p->popup = (CompSpec){ "popup", 5, popup_cols };

    p->fishTex = LoadTexture("assets/fish.png"); // id 0 (skip drawing) if missing
    for (int i = 0; i < FISH_SPECIES_COUNT; i++) {
        if (SPECIES[i].sprite) p->speciesTex[i] = LoadTexture(SPECIES[i].sprite);
    }

    CompSpec inv_specs[2] = { p->fish, p->inventory };
    if (query_init(&p->inventory_query, db, inv_specs, 2) != 0) return -1;
    return 0;
}

void fishing_plugin_destroy(FishingPlugin *p)
{
    query_destroy(&p->inventory_query);
    for (int i = 0; i < FISH_SPECIES_COUNT; i++) {
        if (p->speciesTex[i].id != 0) UnloadTexture(p->speciesTex[i]);
    }
    if (p->fishTex.id != 0) UnloadTexture(p->fishTex);
}
