#ifndef ECS_H
#define ECS_H

#include "query.h"
#include "sqlite3.h"
#include <stdint.h>

int ecs_setup(sqlite3 *db);
int64_t ecs_entity_create(sqlite3 *db);
int ecs_component_set(sqlite3 *db, const CompSpec *spec, int64_t entity_id, const float *data);

#endif
