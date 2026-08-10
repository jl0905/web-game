#include "plugin_game.h"

#include "ecs.h"
#include "physics.h"
#include "plugin_economy.h"
#include "plugin_fishing.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

static const char *pos_cols[] = { "x", "y" };
static const char *vel_cols[] = { "x", "y" };
static const char *rect_cols[] = { "x", "y", "w", "h" };
static const char *color_cols[] = { "r", "g", "b", "a" };

// --- Movement system ---------------------------------------------------------
// The player's position/velocity live in the ECS; the movement query streams
// every entity with position + velocity + square components, runs the physics
// integrate/bounds steps, and writes the results back with prepared UPDATEs.

struct GameUpdateSystem {
    Query query;
    sqlite3_stmt *upd_pos;
    sqlite3_stmt *upd_vel;
    int dim;
    PhysicsParams params;
    float lo[PHYS_MAX_DIM];
    float hi[PHYS_MAX_DIM];
};

typedef enum {
    COMP_POSITION = 0,
    COMP_VELOCITY = 1,
    COMP_SQUARE   = 2,
} MovementComponent;

typedef struct {
    int dim;
    PhysicsParams params;
    float lo[PHYS_MAX_DIM];
    float hi[PHYS_MAX_DIM];
    CompSpec pos;
    CompSpec vel;
    const char *tag_table;
} UpdateConfig;

static int prepare_update(sqlite3 *db, sqlite3_stmt **out, const CompSpec *spec)
{
    char sql[192];
    int off = snprintf(sql, sizeof sql, "UPDATE %s SET ", spec->table);
    for (int i = 0; i < spec->ncols; i++)
        off += snprintf(sql + off, sizeof sql - (size_t)off,
                        "%s%s=?%d", i > 0 ? "," : "", spec->cols[i], i + 1);
    off += snprintf(sql + off, sizeof sql - (size_t)off,
                    " WHERE entity_id=?%d", spec->ncols + 1);

    if (sqlite3_prepare_v2(db, sql, -1, out, NULL) != SQLITE_OK) {
        fprintf(stderr, "prepare_update(%s): %s\n", spec->table, sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

static int update_system_init(GameUpdateSystem *s, sqlite3 *db, const UpdateConfig *cfg)
{
    if (cfg->dim < 1 || cfg->dim > PHYS_MAX_DIM) {
        fprintf(stderr, "update_system_init: dim %d out of range [1, %d]\n", cfg->dim, PHYS_MAX_DIM);
        return -1;
    }
    if (cfg->dim != cfg->pos.ncols || cfg->dim != cfg->vel.ncols) {
        fprintf(stderr, "update_system_init: dim %d does not match component column counts\n", cfg->dim);
        return -1;
    }

    CompSpec specs[3] = {
        cfg->pos,
        cfg->vel,
        { cfg->tag_table, 0, NULL },
    };

    s->dim = cfg->dim;
    s->params = cfg->params;
    for (int i = 0; i < cfg->dim; i++) {
        s->lo[i] = cfg->lo[i];
        s->hi[i] = cfg->hi[i];
    }

    if (query_init(&s->query, db, specs, 3) != 0) return -1;
    if (prepare_update(db, &s->upd_pos, &cfg->pos) != 0) return -1;
    if (prepare_update(db, &s->upd_vel, &cfg->vel) != 0) return -1;
    return 0;
}

static void update_system_run(GameUpdateSystem *s, float dt, const float *input)
{
    Query *q = &s->query;
    query_reset(q);

    while (query_next(q)) {
        float pos[PHYS_MAX_DIM];
        float vel[PHYS_MAX_DIM];
        for (int i = 0; i < s->dim; i++) {
            pos[i] = (float)q->tables[COMP_POSITION].data[i];
            vel[i] = (float)q->tables[COMP_VELOCITY].data[i];
        }

        integrate(dt, input, pos, vel, s->dim, &s->params);
        collide_bounds(pos, vel, s->dim, s->lo, s->hi);

        sqlite3_reset(s->upd_pos);
        for (int i = 0; i < s->dim; i++)
            sqlite3_bind_double(s->upd_pos, i + 1, pos[i]);
        sqlite3_bind_int(s->upd_pos, s->dim + 1, q->entity_id);
        sqlite3_step(s->upd_pos);

        sqlite3_reset(s->upd_vel);
        for (int i = 0; i < s->dim; i++)
            sqlite3_bind_double(s->upd_vel, i + 1, vel[i]);
        sqlite3_bind_int(s->upd_vel, s->dim + 1, q->entity_id);
        sqlite3_step(s->upd_vel);
    }
}

static void update_system_destroy(GameUpdateSystem *s)
{
    query_destroy(&s->query);
    sqlite3_finalize(s->upd_pos);
    sqlite3_finalize(s->upd_vel);
}

// --- ECS accessors -----------------------------------------------------------

Rectangle game_plugin_rect(const GamePlugin *p, int64_t id)
{
    float d[4];
    if (ecs_component_get(p->db, &p->rect, id, d) != 0)
        return (Rectangle){ 0, 0, 0, 0 };
    return (Rectangle){ d[0], d[1], d[2], d[3] };
}

Color game_plugin_color(const GamePlugin *p, int64_t id)
{
    float d[4];
    if (ecs_component_get(p->db, &p->color, id, d) != 0)
        return RAYWHITE;
    return (Color){ (unsigned char)d[0], (unsigned char)d[1],
                    (unsigned char)d[2], (unsigned char)d[3] };
}

static void game_read_player(GamePlugin *p)
{
    float pos[2], vel[2];
    ecs_component_get(p->db, &p->pos, p->player_id, pos);
    ecs_component_get(p->db, &p->vel, p->player_id, vel);
    p->px = pos[0]; p->py = pos[1];
    p->vx = vel[0]; p->vy = vel[1];
}

static void game_write_player(GamePlugin *p)
{
    ecs_component_set(p->db, &p->pos, p->player_id, (float[]){ p->px, p->py });
    ecs_component_set(p->db, &p->vel, p->player_id, (float[]){ p->vx, p->vy });
}

// Push the player out of every solid rectangle (lake, shop) and write any
// correction back into the ECS. The world boundary is handled by the physics
// bounds in the update system, not by push-out collision.
static void game_collide_solids(GamePlugin *p)
{
    bool pushed = false;
    query_reset(&p->solids);
    while (query_next(&p->solids)) {
        RectF solid = {
            (float)p->solids.tables[0].data[0],
            (float)p->solids.tables[0].data[1],
            (float)p->solids.tables[0].data[2],
            (float)p->solids.tables[0].data[3],
        };
        float pos[2] = { p->px, p->py };
        float vel[2] = { p->vx, p->vy };
        if (collide_solid(pos, vel, p->player_size, &solid)) {
            p->px = pos[0]; p->py = pos[1];
            p->vx = vel[0]; p->vy = vel[1];
            pushed = true;
        }
    }
    if (pushed) game_write_player(p);
}

// --- Lifecycle ---------------------------------------------------------------

int game_plugin_init(GamePlugin *p, sqlite3 *db)
{
    p->db = db;
    p->plate_w = PLATE_W;
    p->plate_h = PLATE_H;
    p->player_size = PLAYER_SIZE;

    p->pos = (CompSpec){ "position", 2, pos_cols };
    p->vel = (CompSpec){ "velocity", 2, vel_cols };
    p->square = (CompSpec){ "square", 0, NULL };
    p->rect = (CompSpec){ "rectangle", 4, rect_cols };
    p->color = (CompSpec){ "color", 4, color_cols };
    p->solid = (CompSpec){ "solid", 0, NULL };

    p->px = 380.0f;
    p->py = 230.0f;

    // The world boundary entity's rectangle (set by main) mirrors these plate
    // constants, so the physics bounds stay in sync with the drawn wall.
    UpdateConfig cfg = {
        .dim = 2,
        .params = { ACCEL, MAX_SPEED, FRICTION },
        .lo = { 0.0f, 0.0f },
        .hi = { PLATE_W - PLAYER_SIZE, PLATE_H - PLAYER_SIZE },
        .pos = p->pos,
        .vel = p->vel,
        .tag_table = "square",
    };
    p->update = calloc(1, sizeof(GameUpdateSystem));
    if (p->update == NULL || update_system_init(p->update, db, &cfg) != 0) {
        free(p->update);
        p->update = NULL;
        return -1;
    }

    CompSpec solid_specs[2] = { p->rect, p->solid };
    if (query_init(&p->solids, db, solid_specs, 2) != 0) {
        update_system_destroy(p->update);
        free(p->update);
        p->update = NULL;
        return -1;
    }
    return 0;
}

void game_plugin_update(GamePlugin *p, float dt,
                        FishingPlugin *fishing, EconomyPlugin *economy)
{
    float input[2] = {
        (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
          - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)),
        (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
          - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)),
    };

    update_system_run(p->update, dt, input);
    game_read_player(p);
    game_collide_solids(p);

    if (IsKeyPressed(KEY_E)) {
        if (!fishing_plugin_try_cast(fishing, p->px, p->py, p->player_size))
            economy_plugin_try_sell(economy);
    }
}

void game_plugin_draw(GamePlugin *p)
{
    Color body = game_plugin_color(p, p->player_id);

    DrawRectangleRec((Rectangle){ p->px + 4, p->py + 6, p->player_size, p->player_size },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec((Rectangle){ p->px, p->py, p->player_size, p->player_size }, body);
    DrawRectangleLinesEx((Rectangle){ p->px + 1.5f, p->py + 1.5f, p->player_size - 3, p->player_size - 3 },
                         3.0f, (Color){ 138, 106, 21, 255 });
}

void game_plugin_destroy(GamePlugin *p)
{
    query_destroy(&p->solids);
    if (p->update) {
        update_system_destroy(p->update);
        free(p->update);
    }
}
