#include "plugin_economy.h"

#include "ecs.h"
#include "plugin_fishing.h"
#include "plugin_game.h"
#include "raylib.h"
#include "ui.h"
#include <stdio.h>

static const char *wallet_cols[] = { "coins" };

// --- Lifecycle ---------------------------------------------------------------

int economy_plugin_init(EconomyPlugin *p, sqlite3 *db, GamePlugin *game,
                        FishingPlugin *fishing)
{
    p->db = db;
    p->game = game;
    p->fishing = fishing;
    p->wallet = (CompSpec){ "wallet", 1, wallet_cols };
    return 0;
}

void economy_plugin_destroy(EconomyPlugin *p)
{
    (void)p; // the wallet component lives on the player entity (ECS)
}

// --- Shop --------------------------------------------------------------------

static void readCoins(const EconomyPlugin *p, int *out)
{
    float d[1];
    ecs_component_get(p->db, &p->wallet, p->game->player_id, d);
    *out = (int)d[0];
}

static void writeCoins(const EconomyPlugin *p, int coins)
{
    ecs_component_set(p->db, &p->wallet, p->game->player_id, (float[]){ (float)coins });
}

int economy_plugin_try_sell(EconomyPlugin *p)
{
    FishingPlugin *f = p->fishing;
    if (!fishing_plugin_near_shop(f, p->game->px, p->game->py, p->game->player_size))
        return 0;

    int64_t ids[MAX_INVENTORY];
    int count = 0;
    float total = 0.0f;

    query_reset(&f->inventory_query);
    while (query_next(&f->inventory_query) && count < MAX_INVENTORY) {
        ids[count] = f->inventory_query.entity_id;
        Fish fish = {
            (int)f->inventory_query.tables[0].data[0],
            (float)f->inventory_query.tables[0].data[1],
            f->inventory_query.tables[0].data[2] != 0.0f,
        };
        total += fishing_plugin_fish_price(&fish);
        count++;
    }

    if (count == 0) {
        fishing_plugin_show_popup(f, "Shop: no fish to sell!", RAYWHITE, NULL);
        return 1;
    }

    for (int i = 0; i < count; i++) ecs_entity_destroy(p->db, ids[i]);

    int coins;
    readCoins(p, &coins);
    writeCoins(p, coins + (int)total);

    char buf[128];
    snprintf(buf, sizeof(buf), "Sold %d fish for %d coins!", count, (int)total);
    fishing_plugin_show_popup(f, buf, GOLD, NULL);
    return 1;
}

// --- HUD ---------------------------------------------------------------------

void economy_plugin_draw(const EconomyPlugin *p)
{
    int coins;
    readCoins(p, &coins);
    char buf[64];
    snprintf(buf, sizeof(buf), "Coins: %d", coins);
    ui_text(buf, 12, 10, 22, GOLD);
    snprintf(buf, sizeof(buf), "Fish: %d/%d",
             fishing_plugin_inventory_count(p->fishing), MAX_INVENTORY);
    ui_text(buf, 12, 34, 22, SKYBLUE);
}
