#ifndef PLUGIN_GAME_H
#define PLUGIN_GAME_H

#include "query.h"
#include "raylib.h"
#include "sqlite3.h"
#include <stdint.h>

// The main-game plugin owns the generic world primitives (position, velocity,
// squares, rectangles, colors, solids), the player and world-boundary entities,
// and the movement systems (physics integrate + solid push-out) that act on
// them. main() registers the plugin, builds the player/boundary entities from
// the exposed component definitions, then drives game_plugin_update() while the
// player is walking.

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f
#define INTERACT_RANGE 28.0f

typedef struct FishingPlugin FishingPlugin;
typedef struct EconomyPlugin EconomyPlugin;

typedef struct GameUpdateSystem GameUpdateSystem;

typedef struct GamePlugin {
    sqlite3 *db;

    // Component definitions for the primitive world. Scene entities are built
    // by main() after init using these.
    CompSpec pos;    // position {x,y}
    CompSpec vel;    // velocity {x,y}
    CompSpec square; // square (tag)
    CompSpec rect;   // rectangle {x,y,w,h}
    CompSpec color;  // color {r,g,b,a}
    CompSpec solid;  // solid (tag)

    // Plate/world constants. The boundary entity's rectangle mirrors these so
    // the physics bounds and the drawn wall stay in sync.
    float plate_w;
    float plate_h;
    float player_size;

    // Scene entities, created by main() after init.
    int64_t player_id;
    int64_t boundary_id;

    // Player state mirrored out of the ECS every frame so plugins can read it
    // without re-running a query.
    float px, py, vx, vy;

    // Internal movement systems.
    GameUpdateSystem *update;
    Query solids; // rectangle + solid (the push-out collision targets)
} GamePlugin;

int game_plugin_init(GamePlugin *p, sqlite3 *db);
// Runs while the player is in STATE_WALK: physics integrate, solid push-out,
// then interaction (cast line / sell). fishing and economy are the interaction
// targets.
void game_plugin_update(GamePlugin *p, float dt,
                        FishingPlugin *fishing, EconomyPlugin *economy);
void game_plugin_draw(GamePlugin *p);
void game_plugin_destroy(GamePlugin *p);

// ECS read helpers for the primitive components (used by drawing code in the
// other plugins).
Rectangle game_plugin_rect(const GamePlugin *p, int64_t id);
Color game_plugin_color(const GamePlugin *p, int64_t id);

#endif
