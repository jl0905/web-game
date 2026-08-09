#ifndef UPDATE_SYSTEM_H
#define UPDATE_SYSTEM_H

#include "physics.h"
#include "query.h"
#include "sqlite3.h"

typedef struct {
    int dim;
    const char *const *pos_cols;
    const char *const *vel_cols;
    PhysicsParams params;
    float lo[PHYS_MAX_DIM];
    float hi[PHYS_MAX_DIM];
} UpdateSystemConfig;

typedef struct {
    sqlite3 *db;
    Query query;
    sqlite3_stmt *upd_pos;
    sqlite3_stmt *upd_vel;
    int dim;
    PhysicsParams params;
    float lo[PHYS_MAX_DIM];
    float hi[PHYS_MAX_DIM];
} UpdateSystem;

int update_system_init(UpdateSystem *s, sqlite3 *db, const UpdateSystemConfig *cfg);
void update_system_run(UpdateSystem *s, float dt, const float *input);
void update_system_destroy(UpdateSystem *s);

#endif
