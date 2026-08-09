#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f
#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

// Lake on the right side of the baseplate
#define LAKE_X 560.0f
#define LAKE_Y 90.0f
#define LAKE_W 220.0f
#define LAKE_H 330.0f

// Shop in the top-left corner
#define SHOP_X 30.0f
#define SHOP_Y 30.0f
#define SHOP_W 120.0f
#define SHOP_H 90.0f

#define INTERACT_RANGE 28.0f
#define MAX_INVENTORY 32
#define SHINY_ODDS 100 // 1 in N catches

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

static float px = 380.0f, py = 230.0f;
static float vx = 0.0f, vy = 0.0f;

static GameState state = STATE_WALK;
static float stateTimer = 0.0f;
static float biteAfter = 0.0f;

static Fish hooked;
static Fish inventory[MAX_INVENTORY];
static int inventoryCount = 0;
static int coins = 0;

static char popupText[128] = "";
static Color popupColor = WHITE;
static bool popupShowFish = false; // show the fish sprite (catch popups only)
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

// Minigame: vertical track; keep the catch bar over the darting fish.
#define TRACK_H 260.0f
#define BAR_H 70.0f
static float fishPos = 0.5f;    // 0..1 on track (0 = bottom)
static float fishTarget = 0.5f;
static float fishVel = 0.0f;
static float barPos = 0.3f;     // bottom of catch bar, 0..1
static float barVel = 0.0f;
static float catchProgress = 0.4f;

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

static bool insideRect(Rectangle r)
{
    return CheckCollisionRecs(playerRect(), r);
}

static Rectangle lakeRect(void) { return (Rectangle){ LAKE_X, LAKE_Y, LAKE_W, LAKE_H }; }
static Rectangle shopRect(void) { return (Rectangle){ SHOP_X, SHOP_Y, SHOP_W, SHOP_H }; }

static void showPopup(const char *text, Color c)
{
    snprintf(popupText, sizeof(popupText), "%s", text);
    popupColor = c;
    popupShowFish = false;
    state = STATE_POPUP;
    stateTimer = 0.0f;
}

// --- Update ------------------------------------------------------------------

static void updateWalk(float dt)
{
    float inx = (float)((IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                      - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)));
    float iny = (float)((IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
                      - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)));

    float len = sqrtf(inx * inx + iny * iny);
    if (len > 1.0f) { inx /= len; iny /= len; }

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

    // The lake and the shop are solid
    Rectangle solids[2] = { lakeRect(), shopRect() };
    for (int i = 0; i < 2; i++) {
        if (!insideRect(solids[i])) continue;
        Rectangle r = solids[i];
        // Push out along the smallest overlap
        float overL = (px + PLAYER_SIZE) - r.x;
        float overR = (r.x + r.width) - px;
        float overT = (py + PLAYER_SIZE) - r.y;
        float overB = (r.y + r.height) - py;
        float m = fminf(fminf(overL, overR), fminf(overT, overB));
        if (m == overL) { px = r.x - PLAYER_SIZE; vx = 0.0f; }
        else if (m == overR) { px = r.x + r.width; vx = 0.0f; }
        else if (m == overT) { py = r.y - PLAYER_SIZE; vy = 0.0f; }
        else { py = r.y + r.height; vy = 0.0f; }
    }

    if (IsKeyPressed(KEY_E)) {
        if (nearRect(lakeRect())) {
            state = STATE_WAITING;
            stateTimer = 0.0f;
            biteAfter = 1.0f + rand01() * 3.0f;
        } else if (nearRect(shopRect())) {
            if (inventoryCount == 0) {
                showPopup("Shop: no fish to sell!", RAYWHITE);
            } else {
                float total = 0.0f;
                for (int i = 0; i < inventoryCount; i++) total += fishPrice(inventory[i]);
                int sold = inventoryCount;
                coins += (int)total;
                inventoryCount = 0;
                char buf[128];
                snprintf(buf, sizeof(buf), "Sold %d fish for %d coins!", sold, (int)total);
                showPopup(buf, GOLD);
            }
        }
    }
}

static void startMinigame(void)
{
    hooked = rollFish();
    state = STATE_MINIGAME;
    fishPos = 0.5f; fishTarget = 0.5f; fishVel = 0.0f;
    barPos = 0.3f; barVel = 0.0f;
    catchProgress = 0.4f;
}

static void updateFishing(float dt)
{
    stateTimer += dt;

    if (state == STATE_WAITING) {
        if (IsKeyPressed(KEY_E)) { state = STATE_WALK; return; } // reel in early
        if (stateTimer >= biteAfter) { state = STATE_BITE; stateTimer = 0.0f; }
        return;
    }

    if (state == STATE_BITE) {
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) { startMinigame(); return; }
        if (stateTimer > 0.7f) {
            showPopup("It got away...", LIGHTGRAY);
        }
        return;
    }

    if (state == STATE_MINIGAME) {
        // Fish darts toward a wandering target; harder species dart faster
        float difficulty = 0.6f + 0.4f * (float)hooked.species; // 0.6 .. 2.2
        if (rand01() < dt * (0.8f + 0.5f * difficulty)) fishTarget = rand01();
        float accel = (fishTarget - fishPos) * (3.0f + 2.0f * difficulty);
        fishVel += accel * dt;
        fishVel *= 1.0f / (1.0f + 2.0f * dt);
        fishPos = clampf(fishPos + fishVel * dt, 0.0f, 1.0f);

        // Catch bar: hold SPACE to rise, gravity pulls down (Stardew-style)
        float barSpan = BAR_H / TRACK_H;
        if (IsKeyDown(KEY_SPACE)) barVel += 3.2f * dt;
        else barVel -= 3.2f * dt;
        barVel = clampf(barVel, -1.4f, 1.4f);
        barPos += barVel * dt;
        if (barPos < 0.0f) { barPos = 0.0f; barVel = 0.0f; }
        if (barPos > 1.0f - barSpan) { barPos = 1.0f - barSpan; barVel = 0.0f; }

        bool overlap = fishPos >= barPos && fishPos <= barPos + barSpan;
        catchProgress += (overlap ? 0.35f : -0.25f) * dt;

        if (catchProgress >= 1.0f) {
            if (inventoryCount >= MAX_INVENTORY) {
                showPopup("Caught it - but your bag is full!", ORANGE);
            } else {
                inventory[inventoryCount++] = hooked;
                char buf[128];
                snprintf(buf, sizeof(buf), "Caught a %s%s! %.0f cm - worth %d coins",
                         hooked.shiny ? "SHINY " : "", SPECIES[hooked.species].name,
                         hooked.length, (int)fishPrice(hooked));
                showPopup(buf, hooked.shiny ? GOLD : SKYBLUE);
                popupShowFish = true;
            }
        } else if (catchProgress <= 0.0f) {
            showPopup("The fish escaped!", LIGHTGRAY);
        }
        return;
    }

    if (state == STATE_POPUP) {
        if (stateTimer > 0.6f &&
            (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || stateTimer > 3.0f)) {
            state = STATE_WALK;
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
    Rectangle lake = lakeRect();
    DrawRectangleRounded(lake, 0.25f, 8, (Color){ 38, 92, 150, 255 });
    double tnow = GetTime();
    for (int i = 0; i < 4; i++) {
        float wy = lake.y + 40.0f + 70.0f * i + 6.0f * sinf((float)tnow * 1.5f + i * 1.7f);
        DrawLineEx((Vector2){ lake.x + 20, wy }, (Vector2){ lake.x + lake.width - 20, wy },
                   2.0f, (Color){ 255, 255, 255, 40 });
    }

    // Shop
    Rectangle shop = shopRect();
    DrawRectangleRec(shop, (Color){ 120, 80, 48, 255 });
    DrawRectangleRec((Rectangle){ shop.x, shop.y, shop.width, 22 }, (Color){ 165, 42, 42, 255 });
    drawText("FISH SHOP", shop.x + 14, shop.y + 2, 18, RAYWHITE);
    DrawRectangleLinesEx(shop, 3.0f, (Color){ 60, 40, 24, 255 });

    DrawRectangleLinesEx((Rectangle){ 2, 2, PLATE_W - 4, PLATE_H - 4 }, 4.0f,
                         (Color){ 0, 0, 0, 89 });
}

static void drawPlayer(void)
{
    DrawRectangleRec((Rectangle){ px + 4, py + 6, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec(playerRect(), (Color){ 228, 179, 48, 255 });
    DrawRectangleLinesEx((Rectangle){ px + 1.5f, py + 1.5f, PLAYER_SIZE - 3, PLAYER_SIZE - 3 },
                         3.0f, (Color){ 138, 106, 21, 255 });

    // Fishing rod + line while fishing
    if (state == STATE_WAITING || state == STATE_BITE || state == STATE_MINIGAME) {
        Rectangle lake = lakeRect();
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
    float trackX = PLATE_W - 70.0f;
    float trackY = (PLATE_H - TRACK_H) / 2.0f;

    DrawRectangleRec((Rectangle){ trackX - 8, trackY - 8, 40, TRACK_H + 16 },
                     (Color){ 20, 30, 40, 230 });
    // Catch bar
    float barSpan = BAR_H / TRACK_H;
    float barY = trackY + TRACK_H * (1.0f - barPos - barSpan);
    DrawRectangleRec((Rectangle){ trackX - 4, barY, 32, BAR_H }, (Color){ 80, 200, 120, 200 });
    // Fish
    float fy = trackY + TRACK_H * (1.0f - fishPos);
    DrawCircle((int)(trackX + 12), (int)fy, 8.0f,
               hooked.shiny ? GOLD : SPECIES[hooked.species].color);
    // Progress bar
    DrawRectangleRec((Rectangle){ trackX + 36, trackY, 10, TRACK_H }, (Color){ 20, 30, 40, 230 });
    DrawRectangleRec((Rectangle){ trackX + 36, trackY + TRACK_H * (1.0f - catchProgress),
                                  10, TRACK_H * catchProgress },
                     (Color){ 240, 200, 60, 255 });
    drawText("HOLD SPACE", trackX - 40, trackY + TRACK_H + 14, 16, RAYWHITE);
}

static void drawHud(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Coins: %d", coins);
    drawText(buf, 12, 10, 22, GOLD);
    snprintf(buf, sizeof(buf), "Fish: %d/%d", inventoryCount, MAX_INVENTORY);
    drawText(buf, 12, 34, 22, SKYBLUE);

    if (state == STATE_WALK) {
        if (nearRect(lakeRect())) {
            drawText("[E] Cast line", px - 20, py - 26, 18, RAYWHITE);
        } else if (nearRect(shopRect())) {
            drawText("[E] Sell fish", px - 20, py - 26, 18, RAYWHITE);
        }
    } else if (state == STATE_WAITING) {
        drawText("Waiting for a bite... [E] reel in", px - 60, py - 26, 18, RAYWHITE);
    } else if (state == STATE_BITE) {
        drawText("[E] HOOK IT!", px - 20, py - 26, 20, YELLOW);
    }

    if (state == STATE_POPUP) {
        int w = (int)measureText(popupText, 22);
        DrawRectangle((int)(PLATE_W / 2) - w / 2 - 16, 190, w + 32, 46,
                      (Color){ 20, 30, 40, 230 });
        drawText(popupText, PLATE_W / 2.0f - w / 2.0f, 200, 22, popupColor);

        // Hold the catch overhead, Stardew-style (one sprite for all fish
        // for now; per-species sprites later). Shinies get a gold tint.
        Texture2D tex = catchTexture(hooked.species);
        if (popupShowFish && tex.id != 0) {
            float s = 64.0f / (float)tex.height; // ~64 px tall on screen
            float fw = tex.width * s, fh = tex.height * s;
            float bob = 4.0f * sinf((float)GetTime() * 4.0f);
            DrawTexturePro(tex,
                           (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                           (Rectangle){ px + PLAYER_SIZE / 2.0f - fw / 2.0f,
                                        py - fh - 12.0f + bob, fw, fh },
                           (Vector2){ 0, 0 }, 0.0f,
                           hooked.shiny ? GOLD : WHITE);
        }
    }

    drawText("WASD move - E interact", 12, PLATE_H - 28, 18,
             (Color){ 255, 255, 255, 140 });
}

int main(void)
{
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

        if (state == STATE_WALK) updateWalk(dt);
        else updateFishing(dt);

        BeginTextureMode(target);
        drawWorld();
        drawPlayer();
        if (state == STATE_MINIGAME) drawMinigame();
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
    return 0;
}
