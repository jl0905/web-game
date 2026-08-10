#ifndef PLUGIN_FISHING_H
#define PLUGIN_FISHING_H

#include "query.h"
#include "raylib.h"
#include "sqlite3.h"
#include <stdbool.h>
#include <stdint.h>

// The fishing plugin owns everything about catching fish: the species table and
// pricing, the lake and shop objects, the fishing state machine, the minigame,
// the popup, and the caught-fish bag (inventory). It depends on the game plugin
// for the primitive components and the player's mirrored position.

#define MAX_INVENTORY 32
#define SHINY_ODDS 100 // 1 in N catches
#define FISH_SPECIES_COUNT 6

typedef struct GamePlugin GamePlugin;

typedef enum {
    STATE_WALK,
    STATE_WAITING,  // line cast, waiting for a bite
    STATE_BITE,     // "!" window — hook now or lose it
    STATE_MINIGAME, // tension bar
    STATE_POPUP,    // caught/escaped/sold message
} GameState;

typedef struct {
    const char *name;
    float basePrice;   // price of an average-length specimen
    float minLen;      // cm
    float maxLen;      // cm
    int weight;        // drop-table weight (higher = more common)
    Color color;
    const char *sprite; // per-species sprite; NULL = default fish sprite
} FishSpecies;

typedef struct {
    int species;
    float length; // cm
    bool shiny;
} Fish;

typedef struct FishingPlugin {
    sqlite3 *db;
    GamePlugin *game;

    // Component definitions. Scene entities are built by main() after init.
    CompSpec fish;      // fish {species,length,shiny}
    CompSpec inventory; // inventory (tag) — a caught fish still in the bag
    CompSpec game_comp; // game {state,timer,bite_after} — the fishing state machine
    CompSpec minigame;  // minigame {fish_pos,...}
    CompSpec popup;     // popup {r,g,b,a,show_fish}

    // Scene entities, created by main() after init.
    int64_t lake_id;
    int64_t shop_id;
    int64_t state_id; // the fishing state-machine/minigame/hooked-fish entity
    int64_t popup_id; // the "fish above the head" icon

    // Sprites; species 0 ("Pond Minnow") has no sprite and falls back to
    // fishTex.
    Texture2D fishTex;
    Texture2D speciesTex[FISH_SPECIES_COUNT];

    Query inventory_query; // every caught fish still in the bag
} FishingPlugin;

int fishing_plugin_init(FishingPlugin *p, sqlite3 *db, GamePlugin *game);
// Runs the fishing state machine (STATE_WAITING / BITE / MINIGAME / POPUP).
void fishing_plugin_update(FishingPlugin *p, float dt);
void fishing_plugin_draw_world(FishingPlugin *p);
void fishing_plugin_draw_hud(FishingPlugin *p);
void fishing_plugin_destroy(FishingPlugin *p);

// Current fishing-game state (STATE_WALK when not fishing).
int fishing_plugin_state(const FishingPlugin *p);
// Player pressed E near the lake: cast a line. Returns 1 if the cast started.
int fishing_plugin_try_cast(FishingPlugin *p, float px, float py, float size);
// Overlay a message (sold/caught/escaped) for a few seconds. Pass fish = NULL
// to hide the caught-fish icon.
void fishing_plugin_show_popup(FishingPlugin *p, const char *text, Color c,
                               const Fish *fish);
// Number of fish currently in the bag (caught, not yet sold).
int fishing_plugin_inventory_count(FishingPlugin *p);
// Is the given player square within interact range of the lake / shop?
int fishing_plugin_near_lake(const FishingPlugin *p, float px, float py, float size);
int fishing_plugin_near_shop(const FishingPlugin *p, float px, float py, float size);

// price = speciesBase * lengthFactor * (shiny ? 10 : 1)   (see DESIGN.md)
float fishing_plugin_fish_price(const Fish *f);

#endif
