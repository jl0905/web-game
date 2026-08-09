#include "ecs.h"

#include <stdio.h>

int ecs_setup(sqlite3 *db)
{
    char *err = NULL;
    const char *sql =
        "CREATE TABLE entities (id INTEGER PRIMARY KEY);"
        "CREATE TABLE position (entity_id INTEGER PRIMARY KEY, x REAL NOT NULL, y REAL NOT NULL);"
        "CREATE TABLE velocity (entity_id INTEGER PRIMARY KEY, x REAL NOT NULL, y REAL NOT NULL);"
        "CREATE TABLE square (entity_id INTEGER PRIMARY KEY);";
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "ecs_setup: %s\n", err ? err : "unknown error");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int64_t ecs_entity_create(sqlite3 *db)
{
    if (sqlite3_exec(db, "INSERT INTO entities (id) VALUES (NULL)", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_entity_create: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return sqlite3_last_insert_rowid(db);
}

int ecs_component_set(sqlite3 *db, const CompSpec *spec, int64_t entity_id, const float *data)
{
    char sql[192];
    int off = snprintf(sql, sizeof sql, "INSERT OR REPLACE INTO %s (entity_id", spec->table);
    for (int c = 0; c < spec->ncols; c++)
        off += snprintf(sql + off, sizeof sql - (size_t)off, ", %s", spec->cols[c]);
    off += snprintf(sql + off, sizeof sql - (size_t)off, ") VALUES (?1");
    for (int c = 0; c < spec->ncols; c++)
        off += snprintf(sql + off, sizeof sql - (size_t)off, ", ?%d", c + 2);
    off += snprintf(sql + off, sizeof sql - (size_t)off, ")");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_component_set(%s): %s\n", spec->table, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, entity_id);
    for (int c = 0; c < spec->ncols; c++)
        sqlite3_bind_double(stmt, c + 2, data[c]);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "ecs_component_set(%s): %s\n", spec->table, sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}
