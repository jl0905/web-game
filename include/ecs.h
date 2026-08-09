#ifndef ECS_H
#define ECS_H

#include "query.h"
#include "sqlite3.h"
#include <stddef.h>
#include <stdint.h>

int ecs_setup(sqlite3 *db);
int64_t ecs_entity_create(sqlite3 *db);
int ecs_component_set(sqlite3 *db, const CompSpec *spec, int64_t entity_id, const float *data);
int ecs_component_get(sqlite3 *db, const CompSpec *spec, int64_t entity_id, float *data);
int ecs_text_set(sqlite3 *db, const char *table, const char *col, int64_t entity_id,
                 const char *value);
int ecs_text_get(sqlite3 *db, const char *table, const char *col, int64_t entity_id,
                 char *out, size_t n);
int ecs_entity_destroy(sqlite3 *db, int64_t entity_id);

#endif
