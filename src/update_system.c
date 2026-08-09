#include "update_system.h"

#include <stdio.h>

typedef enum {
    COMP_POSITION = 0,
    COMP_VELOCITY = 1,
    COMP_SQUARE   = 2,
} Component;

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

int update_system_init(UpdateSystem *s, sqlite3 *db, const UpdateSystemConfig *cfg)
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

void update_system_run(UpdateSystem *s, float dt, const float *input)
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

void update_system_destroy(UpdateSystem *s)
{
    query_destroy(&s->query);
    sqlite3_finalize(s->upd_pos);
    sqlite3_finalize(s->upd_vel);
}
