#ifndef PLUGIN_ECONOMY_H
#define PLUGIN_ECONOMY_H

#include "query.h"
#include "sqlite3.h"

// The economy plugin owns money and the shop: the wallet component on the
// player, selling caught fish, and the coins / fish-caught HUD. It reads the
// caught-fish bag from the fishing plugin's inventory.

typedef struct GamePlugin GamePlugin;
typedef struct FishingPlugin FishingPlugin;

typedef struct EconomyPlugin {
    sqlite3 *db;
    GamePlugin *game;
    FishingPlugin *fishing;

    // Component definition for the player's wallet; main() attaches it to the
    // player entity after init.
    CompSpec wallet; // wallet {coins}
} EconomyPlugin;

int economy_plugin_init(EconomyPlugin *p, sqlite3 *db, GamePlugin *game,
                        FishingPlugin *fishing);
void economy_plugin_draw(const EconomyPlugin *p);
void economy_plugin_destroy(EconomyPlugin *p);

// Player pressed E near the shop: sell every fish in the bag. Returns 1 if a
// message ("sold" / "nothing to sell") was shown.
int economy_plugin_try_sell(EconomyPlugin *p);

#endif
