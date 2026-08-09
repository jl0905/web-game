#ifndef QUERY_H
#define QUERY_H

#include "sqlite3.h"
#include <stdint.h>

#define QUERY_MAX_TABLES 4
#define QUERY_MAX_COLS 4

typedef struct {
    sqlite3_stmt *stmt;
    int64_t eid;
    double data[QUERY_MAX_COLS];
    int ncols;
    int done;
} QueryTable;

typedef struct {
    const char *table;
    int ncols;
    const char *const *cols;
} CompSpec;

typedef struct {
    QueryTable tables[QUERY_MAX_TABLES];
    int ntables;
    int started;
    int entity_id;
    int done;
} Query;

int query_init(Query *q, sqlite3 *db, const CompSpec *specs, int nspecs);
void query_reset(Query *q);
int query_next(Query *q);
void query_destroy(Query *q);

#endif
