#include "ecs.h"
#include "query.h"
#include "raylib.h"
#include "sqlite3.h"
#include "update_system.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f

#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

#define INTERACT_RANGE 28.0f
#define MAX_INVENTORY 32
#define SHINY_ODDS 100 // 1 in N catches

// Minigame: vertical track; keep the catch bar over the darting fish.
#define TRACK_H 260.0f
#define BAR_H 70.0f

// --- Fish data ---------------------------------------------------------------

typedef struct {
    const char *name;
    float basePrice;   // price of an average-length specimen
    float minLen;      // cm
    float maxLen;      // cm
    int weight;        // drop-table weight (higher = more common)
    Color color;
    const char *sprite; // per-species sprite; NULL = default fish sprite
} FishSpecies;

static const FishSpecies SPECIES[] = {
    { "Pond Minnow",   8.0f,   4.0f,  12.0f, 40, { 176, 196, 222, 255 }, NULL },
    { "Green Perch",  15.0f,  10.0f,  30.0f, 30, { 107, 142,  35, 255 }, NULL },
    { "Amber Carp",   30.0f,  20.0f,  60.0f, 18, { 218, 165,  32, 255 }, NULL },
    { "Blue Pike",    60.0f,  35.0f,  90.0f,  9, {  70, 130, 180, 255 }, NULL },
    { "Squid",        80.0f,  25.0f, 100.0f,  6, { 205, 170, 160, 255 }, "assets/squid.png" },
    { "King Sturgeon", 150.0f, 60.0f, 150.0f, 3, { 147, 112, 219, 255 }, NULL },
};
#define SPECIES_COUNT (int)(sizeof(SPECIES) / sizeof(SPECIES[0]))

typedef struct {
    int species;
    float length; // cm
    bool shiny;
} Fish;

// price = speciesBase * lengthFactor * (shiny ? 10 : 1)   (see DESIGN.md)
static float fishPrice(Fish f)
{
    const FishSpecies *s = &SPECIES[f.species];
    float avg = (s->minLen + s->maxLen) * 0.5f;
    float price = s->basePrice * (f.length / avg);
    if (f.shiny) price *= 10.0f;
    return price;
}

static float rand01(void)
{
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

static Fish rollFish(void)
{
    int total = 0;
    for (int i = 0; i < SPECIES_COUNT; i++) total += SPECIES[i].weight;
    int pick = GetRandomValue(1, total);
    int idx = 0;
    for (int i = 0; i < SPECIES_COUNT; i++) {
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

// --- Game state --------------------------------------------------------------

typedef enum {
    STATE_WALK,
    STATE_WAITING,  // line cast, waiting for a bite
    STATE_BITE,     // "!" window — hook now or lose it
    STATE_MINIGAME, // tension bar
    STATE_POPUP,    // caught/escaped/sold message
} GameState;

// Entities are registered into the ECS in main().
static sqlite3 *g_db = NULL;
static int64_t g_player;
static int64_t g_lake;
static int64_t g_shop;
static int64_t g_boundary;
static int64_t g_fishing; // the fishing minigame entity (game + minigame + hooked fish)
static int64_t g_popup;   // the popup entity ("fish above the head" icon)

// The ECS owns position/velocity; these globals mirror them each frame so the
// game logic and rendering can read the player without re-running the query.
static float px = 380.0f, py = 230.0f;
static float vx = 0.0f, vy = 0.0f;

static Query g_solids;    // rectangle + solid
static Query g_inventory; // fish + inventory (caught fish entities in the bag)

// --- Component specs ---------------------------------------------------------

static const char *pos_cols[] = { "x", "y" };
static CompSpec pos_spec = { "position", 2, pos_cols };
static const char *vel_cols[] = { "x", "y" };
static CompSpec vel_spec = { "velocity", 2, vel_cols };
static CompSpec square_spec = { "square", 0, NULL };
static const char *rect_cols[] = { "x", "y", "w", "h" };
static CompSpec rect_spec = { "rectangle", 4, rect_cols };
static const char *color_cols[] = { "r", "g", "b", "a" };
static CompSpec color_spec = { "color", 4, color_cols };
static CompSpec solid_spec = { "solid", 0, NULL };
static const char *fish_cols[] = { "species", "length", "shiny" };
static CompSpec fish_spec = { "fish", 3, fish_cols };
static const char *game_cols[] = { "state", "timer", "bite_after" };
static CompSpec game_spec = { "game", 3, game_cols };
static const char *minigame_cols[] = { "fish_pos", "fish_target", "fish_vel",
                                       "bar_pos", "bar_vel", "catch_progress" };
static CompSpec minigame_spec = { "minigame", 6, minigame_cols };
static const char *popup_cols[] = { "r", "g", "b", "a", "show_fish" };
static CompSpec popup_spec = { "popup", 5, popup_cols };
static CompSpec inventory_spec = { "inventory", 0, NULL };
static const char *wallet_cols[] = { "coins" };
static CompSpec wallet_spec = { "wallet", 1, wallet_cols };

// --- Component accessors -----------------------------------------------------

static Rectangle entityRect(int64_t id)
{
    float d[4];
    if (ecs_component_get(g_db, &rect_spec, id, d) != 0)
        return (Rectangle){ 0, 0, 0, 0 };
    return (Rectangle){ d[0], d[1], d[2], d[3] };
}

static Color entityColor(int64_t id)
{
    float d[4];
    if (ecs_component_get(g_db, &color_spec, id, d) != 0)
        return RAYWHITE;
    return (Color){ (unsigned char)d[0], (unsigned char)d[1],
                    (unsigned char)d[2], (unsigned char)d[3] };
}

static void readPlayer(void)
{
    float p[2], v[2];
    ecs_component_get(g_db, &pos_spec, g_player, p);
    ecs_component_get(g_db, &vel_spec, g_player, v);
    px = p[0]; py = p[1];
    vx = v[0]; vy = v[1];
}

static void writePlayer(void)
{
    ecs_component_set(g_db, &pos_spec, g_player, (float[]){ px, py });
    ecs_component_set(g_db, &vel_spec, g_player, (float[]){ vx, vy });
}

static void readGame(float out[3])
{
    ecs_component_get(g_db, &game_spec, g_fishing, out);
}

static void writeGame(float state, float timer, float biteAfter)
{
    ecs_component_set(g_db, &game_spec, g_fishing, (float[]){ state, timer, biteAfter });
}

static void readMinigame(float out[6])
{
    ecs_component_get(g_db, &minigame_spec, g_fishing, out);
}

static void writeMinigame(const float in[6])
{
    ecs_component_set(g_db, &minigame_spec, g_fishing, in);
}

static void readHooked(Fish *f)
{
    float d[3];
    ecs_component_get(g_db, &fish_spec, g_fishing, d);
    f->species = (int)d[0];
    f->length = d[1];
    f->shiny = d[2] != 0.0f;
}

static void writeHooked(const Fish *f)
{
    ecs_component_set(g_db, &fish_spec, g_fishing,
                      (float[]){ (float)f->species, f->length, f->shiny ? 1.0f : 0.0f });
}

static void readPopupDisplay(Color *c, bool *showFish)
{
    float d[5];
    ecs_component_get(g_db, &popup_spec, g_popup, d);
    c->r = (unsigned char)d[0];
    c->g = (unsigned char)d[1];
    c->b = (unsigned char)d[2];
    c->a = (unsigned char)d[3];
    *showFish = d[4] != 0.0f;
}

static void writePopupDisplay(Color c, bool showFish)
{
    ecs_component_set(g_db, &popup_spec, g_popup,
                      (float[]){ c.r, c.g, c.b, c.a, showFish ? 1.0f : 0.0f });
}

static void readPopupFish(Fish *f)
{
    float d[3];
    ecs_component_get(g_db, &fish_spec, g_popup, d);
    f->species = (int)d[0];
    f->length = d[1];
    f->shiny = d[2] != 0.0f;
}

static void writePopupFish(const Fish *f)
{
    ecs_component_set(g_db, &fish_spec, g_popup,
                      (float[]){ (float)f->species, f->length, f->shiny ? 1.0f : 0.0f });
}

static void readPopupText(char *out, size_t n)
{
    ecs_text_get(g_db, "popup_text", "text", g_popup, out, n);
}

static void readCoins(int *out)
{
    float d[1];
    ecs_component_get(g_db, &wallet_spec, g_player, d);
    *out = (int)d[0];
}

static void writeCoins(int coins)
{
    ecs_component_set(g_db, &wallet_spec, g_player, (float[]){ (float)coins });
}

// --- Drawing helpers ---------------------------------------------------------

static Texture2D fishTex;
static Texture2D speciesTex[SPECIES_COUNT]; // id 0 → fall back to fishTex
static Font uiFont;

static Texture2D catchTexture(int species)
{
    if (speciesTex[species].id != 0) return speciesTex[species];
    return fishTex;
}

static void drawText(const char *text, float x, float y, float size, Color c)
{
    DrawTextEx(uiFont, text, (Vector2){ x, y }, size, 1.0f, c);
}

static float measureText(const char *text, float size)
{
    return MeasureTextEx(uiFont, text, size, 1.0f).x;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static Rectangle playerRect(void)
{
    return (Rectangle){ px, py, PLAYER_SIZE, PLAYER_SIZE };
}

static bool nearRect(Rectangle r)
{
    Rectangle grown = { r.x - INTERACT_RANGE, r.y - INTERACT_RANGE,
                        r.width + 2 * INTERACT_RANGE, r.height + 2 * INTERACT_RANGE };
    return CheckCollisionRecs(playerRect(), grown);
}

static int inventoryCount(void)
{
    int n = 0;
    query_reset(&g_inventory);
    while (query_next(&g_inventory)) n++;
    return n;
}

static void showPopup(const char *text, Color c, const Fish *f)
{
    writeGame((float)STATE_POPUP, 0.0f, 0.0f);
    ecs_text_set(g_db, "popup_text", "text", g_popup, text);
    writePopupDisplay(c, f != NULL);
    if (f) writePopupFish(f);
}

// --- Update ------------------------------------------------------------------

static void sellFish(void);

// Push the player out of every solid rectangle (lake, shop, boundary) and
// write any correction back into the ECS.
static void collideSolids(void)
{
    bool pushed = false;
    query_reset(&g_solids);
    while (query_next(&g_solids)) {
        RectF solid = {
            (float)g_solids.tables[0].data[0],
            (float)g_solids.tables[0].data[1],
            (float)g_solids.tables[0].data[2],
            (float)g_solids.tables[0].data[3],
        };
        float pos[2] = { px, py };
        float vel[2] = { vx, vy };
        if (collide_solid(pos, vel, PLAYER_SIZE, &solid)) {
            px = pos[0]; py = pos[1];
            vx = vel[0]; vy = vel[1];
            pushed = true;
        }
    }
    if (pushed) writePlayer();
}

static void updateWalk(float dt)
{
    (void)dt; // movement itself is run by the ECS update system

    readPlayer();
    collideSolids();

    if (IsKeyPressed(KEY_E)) {
        if (nearRect(entityRect(g_lake))) {
            writeGame((float)STATE_WAITING, 0.0f, 1.0f + rand01() * 3.0f);
        } else if (nearRect(entityRect(g_shop))) {
            sellFish();
        }
    }
}

static void startMinigame(void)
{
    Fish h = rollFish();
    writeHooked(&h);
    writeGame((float)STATE_MINIGAME, 0.0f, 0.0f);
    float m[6] = { 0.5f, 0.5f, 0.0f, 0.3f, 0.0f, 0.4f };
    writeMinigame(m);
}

static void sellFish(void)
{
    int64_t ids[MAX_INVENTORY];
    int count = 0;
    float total = 0.0f;

    query_reset(&g_inventory);
    while (query_next(&g_inventory) && count < MAX_INVENTORY) {
        ids[count] = g_inventory.entity_id;
        Fish f = {
            (int)g_inventory.tables[0].data[0],
            (float)g_inventory.tables[0].data[1],
            g_inventory.tables[0].data[2] != 0.0f,
        };
        total += fishPrice(f);
        count++;
    }

    if (count == 0) {
        showPopup("Shop: no fish to sell!", RAYWHITE, NULL);
        return;
    }

    for (int i = 0; i < count; i++) ecs_entity_destroy(g_db, ids[i]);

    int coins;
    readCoins(&coins);
    writeCoins(coins + (int)total);

    char buf[128];
    snprintf(buf, sizeof(buf), "Sold %d fish for %d coins!", count, (int)total);
    showPopup(buf, GOLD, NULL);
}

static void updateFishing(float dt)
{
    float g[3];
    readGame(g);
    int state = (int)g[0];
    g[1] += dt;

    if (state == STATE_WAITING) {
        if (IsKeyPressed(KEY_E)) { writeGame((float)STATE_WALK, 0.0f, 0.0f); return; } // reel in early
        if (g[1] >= g[2]) { writeGame((float)STATE_BITE, 0.0f, 0.0f); return; }
        writeGame((float)state, g[1], g[2]);
        return;
    }

    if (state == STATE_BITE) {
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) { startMinigame(); return; }
        if (g[1] > 0.7f) {
            showPopup("It got away...", LIGHTGRAY, NULL);
        } else {
            writeGame((float)state, g[1], g[2]);
        }
        return;
    }

    if (state == STATE_MINIGAME) {
        float m[6];
        readMinigame(m);
        Fish h;
        readHooked(&h);

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

        writeMinigame(m);

        if (m[5] >= 1.0f) {
            if (inventoryCount() >= MAX_INVENTORY) {
                showPopup("Caught it - but your bag is full!", ORANGE, NULL);
            } else {
                // The catch becomes its own entity in the bag.
                int64_t caught = ecs_entity_create(g_db);
                ecs_component_set(g_db, &fish_spec, caught,
                                  (float[]){ (float)h.species, h.length, h.shiny ? 1.0f : 0.0f });
                ecs_component_set(g_db, &inventory_spec, caught, NULL);
                char buf[128];
                snprintf(buf, sizeof(buf), "Caught a %s%s! %.0f cm - worth %d coins",
                         h.shiny ? "SHINY " : "", SPECIES[h.species].name,
                         h.length, (int)fishPrice(h));
                showPopup(buf, h.shiny ? GOLD : SKYBLUE, &h);
            }
        } else if (m[5] <= 0.0f) {
            showPopup("The fish escaped!", LIGHTGRAY, NULL);
        }
        return;
    }

    if (state == STATE_POPUP) {
        if (g[1] > 0.6f &&
            (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || g[1] > 3.0f)) {
            writeGame((float)STATE_WALK, 0.0f, 0.0f);
        } else {
            writeGame((float)state, g[1], g[2]);
        }
        return;
    }
}

// --- Drawing -----------------------------------------------------------------

static void drawWorld(void)
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

    // Lake
    Rectangle lake = entityRect(g_lake);
    DrawRectangleRounded(lake, 0.25f, 8, entityColor(g_lake));
    double tnow = GetTime();
    for (int i = 0; i < 4; i++) {
        float wy = lake.y + 40.0f + 70.0f * i + 6.0f * sinf((float)tnow * 1.5f + i * 1.7f);
        DrawLineEx((Vector2){ lake.x + 20, wy }, (Vector2){ lake.x + lake.width - 20, wy },
                   2.0f, (Color){ 255, 255, 255, 40 });
    }

    // Shop
    Rectangle shop = entityRect(g_shop);
    DrawRectangleRec(shop, entityColor(g_shop));
    DrawRectangleRec((Rectangle){ shop.x, shop.y, shop.width, 22 }, (Color){ 165, 42, 42, 255 });
    char label[64];
    if (ecs_text_get(g_db, "name", "label", g_shop, label, sizeof(label)) == 0) {
        drawText(label, shop.x + 14, shop.y + 2, 18, RAYWHITE);
    }
    DrawRectangleLinesEx(shop, 3.0f, (Color){ 60, 40, 24, 255 });

    // World boundary
    Rectangle bound = entityRect(g_boundary);
    DrawRectangleLinesEx(bound, 4.0f, (Color){ 0, 0, 0, 89 });
}

static void drawPlayer(void)
{
    Color body = entityColor(g_player);

    DrawRectangleRec((Rectangle){ px + 4, py + 6, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec(playerRect(), body);
    DrawRectangleLinesEx((Rectangle){ px + 1.5f, py + 1.5f, PLAYER_SIZE - 3, PLAYER_SIZE - 3 },
                         3.0f, (Color){ 138, 106, 21, 255 });

    // Fishing rod + line while fishing
    float g[3];
    readGame(g);
    int state = (int)g[0];
    if (state == STATE_WAITING || state == STATE_BITE || state == STATE_MINIGAME) {
        Rectangle lake = entityRect(g_lake);
        Vector2 hand = { px + PLAYER_SIZE, py + 8 };
        Vector2 tip = { hand.x + 26, hand.y - 22 };
        Vector2 bobber = { clampf(px + PLAYER_SIZE + 60, lake.x + 15, lake.x + lake.width - 15),
                           clampf(py + 20, lake.y + 15, lake.y + lake.height - 15) };
        DrawLineEx(hand, tip, 3.0f, (Color){ 90, 60, 30, 255 });
        DrawLineEx(tip, bobber, 1.0f, (Color){ 230, 230, 230, 200 });
        DrawCircleV(bobber, 5.0f, state == STATE_BITE ? RED : (Color){ 240, 80, 80, 255 });
        if (state == STATE_BITE) {
            drawText("!", bobber.x - 4, bobber.y - 34, 30, YELLOW);
        }
    }
}

static void drawMinigame(void)
{
    float m[6];
    readMinigame(m);
    Fish h;
    readHooked(&h);

    float trackX = PLATE_W - 70.0f;
    float trackY = (PLATE_H - TRACK_H) / 2.0f;

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
    drawText("HOLD SPACE", trackX - 40, trackY + TRACK_H + 14, 16, RAYWHITE);
}

static void drawHud(void)
{
    int coins;
    readCoins(&coins);
    char buf[64];
    snprintf(buf, sizeof(buf), "Coins: %d", coins);
    drawText(buf, 12, 10, 22, GOLD);
    snprintf(buf, sizeof(buf), "Fish: %d/%d", inventoryCount(), MAX_INVENTORY);
    drawText(buf, 12, 34, 22, SKYBLUE);

    float g[3];
    readGame(g);
    int state = (int)g[0];

    if (state == STATE_WALK) {
        if (nearRect(entityRect(g_lake))) {
            drawText("[E] Cast line", px - 20, py - 26, 18, RAYWHITE);
        } else if (nearRect(entityRect(g_shop))) {
            drawText("[E] Sell fish", px - 20, py - 26, 18, RAYWHITE);
        }
    } else if (state == STATE_WAITING) {
        drawText("Waiting for a bite... [E] reel in", px - 60, py - 26, 18, RAYWHITE);
    } else if (state == STATE_BITE) {
        drawText("[E] HOOK IT!", px - 20, py - 26, 20, YELLOW);
    }

    if (state == STATE_POPUP) {
        char text[128];
        readPopupText(text, sizeof(text));
        Color c;
        bool showFish;
        readPopupDisplay(&c, &showFish);

        int w = (int)measureText(text, 22);
        DrawRectangle((int)(PLATE_W / 2) - w / 2 - 16, 190, w + 32, 46,
                      (Color){ 20, 30, 40, 230 });
        drawText(text, PLATE_W / 2.0f - w / 2.0f, 200, 22, c);

        // Hold the catch overhead, Stardew-style. The popup entity owns the
        // fish being displayed; shinies get a gold tint.
        if (showFish) {
            Fish pf;
            readPopupFish(&pf);
            Texture2D tex = catchTexture(pf.species);
            if (tex.id != 0) {
                float s = 64.0f / (float)tex.height; // ~64 px tall on screen
                float fw = tex.width * s, fh = tex.height * s;
                float bob = 4.0f * sinf((float)GetTime() * 4.0f);
                DrawTexturePro(tex,
                               (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                               (Rectangle){ px + PLAYER_SIZE / 2.0f - fw / 2.0f,
                                            py - fh - 12.0f + bob, fw, fh },
                               (Vector2){ 0, 0 }, 0.0f,
                               pf.shiny ? GOLD : WHITE);
            }
        }
    }

    drawText("WASD move - E interact", 12, PLATE_H - 28, 18,
             (Color){ 255, 255, 255, 140 });
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
    g_db = db;

    // Player
    g_player = ecs_entity_create(db);
    if (g_player < 1) {
        sqlite3_close(db);
        return 1;
    }
    ecs_component_set(db, &pos_spec, g_player, (float[]){ 380.0f, 230.0f });
    ecs_component_set(db, &vel_spec, g_player, (float[]){ 0.0f, 0.0f });
    ecs_component_set(db, &square_spec, g_player, NULL);
    ecs_component_set(db, &color_spec, g_player, (float[]){ 228, 179, 48, 255 });
    ecs_component_set(db, &wallet_spec, g_player, (float[]){ 0 });

    // Lake
    g_lake = ecs_entity_create(db);
    ecs_component_set(db, &rect_spec, g_lake, (float[]){ 560.0f, 90.0f, 220.0f, 330.0f });
    ecs_component_set(db, &solid_spec, g_lake, NULL);
    ecs_component_set(db, &color_spec, g_lake, (float[]){ 38, 92, 150, 255 });
    ecs_text_set(db, "name", "label", g_lake, "LAKE");

    // Shop
    g_shop = ecs_entity_create(db);
    ecs_component_set(db, &rect_spec, g_shop, (float[]){ 30.0f, 30.0f, 120.0f, 90.0f });
    ecs_component_set(db, &solid_spec, g_shop, NULL);
    ecs_component_set(db, &color_spec, g_shop, (float[]){ 120, 80, 48, 255 });
    ecs_text_set(db, "name", "label", g_shop, "FISH SHOP");

    // World boundary (defines the plate and its walls)
    g_boundary = ecs_entity_create(db);
    ecs_component_set(db, &rect_spec, g_boundary, (float[]){ 0.0f, 0.0f, PLATE_W, PLATE_H });
    ecs_component_set(db, &solid_spec, g_boundary, NULL);

    // Fishing minigame entity (state machine + catch bar + hooked fish)
    g_fishing = ecs_entity_create(db);
    ecs_component_set(db, &game_spec, g_fishing, (float[]){ STATE_WALK, 0.0f, 0.0f });
    ecs_component_set(db, &minigame_spec, g_fishing, (float[]){ 0.5f, 0.5f, 0.0f, 0.3f, 0.0f, 0.4f });
    ecs_component_set(db, &fish_spec, g_fishing, (float[]){ 0, 0.0f, 0 });

    // Popup entity (the "fish above the head" icon)
    g_popup = ecs_entity_create(db);
    ecs_component_set(db, &popup_spec, g_popup, (float[]){ 255, 255, 255, 255, 0 });
    ecs_text_set(db, "popup_text", "text", g_popup, "");
    ecs_component_set(db, &fish_spec, g_popup, (float[]){ 0, 0.0f, 0 });

    // Movement system. The world boundary entity's rectangle is the plate.
    Rectangle bound = entityRect(g_boundary);
    UpdateSystemConfig cfg = {
        .dim = 2,
        .params = { ACCEL, MAX_SPEED, FRICTION },
        .lo = { bound.x, bound.y },
        .hi = { bound.x + bound.width - PLAYER_SIZE, bound.y + bound.height - PLAYER_SIZE },
        .pos = pos_spec,
        .vel = vel_spec,
        .tag_table = "square",
    };
    UpdateSystem update_system;
    if (update_system_init(&update_system, db, &cfg) != 0) {
        sqlite3_close(db);
        return 1;
    }

    // Query over every solid rectangle (lake, shop, boundary)
    CompSpec solid_specs[2] = { rect_spec, solid_spec };
    if (query_init(&g_solids, db, solid_specs, 2) != 0) {
        update_system_destroy(&update_system);
        sqlite3_close(db);
        return 1;
    }

    // Query over caught-fish entities still in the bag (fish + inventory)
    CompSpec inv_specs[2] = { fish_spec, inventory_spec };
    if (query_init(&g_inventory, db, inv_specs, 2) != 0) {
        query_destroy(&g_solids);
        update_system_destroy(&update_system);
        sqlite3_close(db);
        return 1;
    }

    // The game simulates and draws in a fixed logical resolution (PLATE_W x
    // PLATE_H) and is scaled to whatever window/monitor size the player picks,
    // letterboxed to preserve aspect.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Emberholm - Fishing Draft");
    SetWindowMinSize(400, 250);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture((int)PLATE_W, (int)PLATE_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    fishTex = LoadTexture("assets/fish.png"); // id 0 (skip drawing) if missing
    for (int i = 0; i < SPECIES_COUNT; i++) {
        if (SPECIES[i].sprite) speciesTex[i] = LoadTexture(SPECIES[i].sprite);
    }
    // Rasterized large and drawn smaller so it stays crisp after window scaling
    uiFont = LoadFontEx("assets/MedievalSharp.ttf", 64, NULL, 0);
    if (uiFont.texture.id == 0) uiFont = GetFontDefault();
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        float g[3];
        readGame(g);
        if ((int)g[0] == STATE_WALK) {
            float input[2] = {
                (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                  - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)),
                (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
                  - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)),
            };

            update_system_run(&update_system, dt, input);
            updateWalk(dt);
        } else {
            updateFishing(dt);
        }

        // State may have changed during the update (e.g. bite -> minigame)
        readGame(g);

        BeginTextureMode(target);
        drawWorld();
        drawPlayer();
        if ((int)g[0] == STATE_MINIGAME) drawMinigame();
        drawHud();
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

    for (int i = 0; i < SPECIES_COUNT; i++) {
        if (speciesTex[i].id != 0) UnloadTexture(speciesTex[i]);
    }
    UnloadTexture(fishTex);
    UnloadRenderTexture(target);
    CloseWindow();
    query_destroy(&g_inventory);
    query_destroy(&g_solids);
    update_system_destroy(&update_system);
    sqlite3_close(db);
    return 0;
}
