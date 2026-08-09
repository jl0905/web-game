#include "ecs.h"

#include <stdio.h>

int ecs_setup(sqlite3 *db)
{
    char *err = NULL;
    const char *sql =
        "CREATE TABLE entities (id INTEGER PRIMARY KEY);"
        "CREATE TABLE position (entity_id INTEGER PRIMARY KEY, x REAL NOT NULL, y REAL NOT NULL);"
        "CREATE TABLE velocity (entity_id INTEGER PRIMARY KEY, x REAL NOT NULL, y REAL NOT NULL);"
        "CREATE TABLE square (entity_id INTEGER PRIMARY KEY);"
        "CREATE TABLE rectangle (entity_id INTEGER PRIMARY KEY, x REAL NOT NULL, y REAL NOT NULL, w REAL NOT NULL, h REAL NOT NULL);"
        "CREATE TABLE color (entity_id INTEGER PRIMARY KEY, r INTEGER NOT NULL, g INTEGER NOT NULL, b INTEGER NOT NULL, a INTEGER NOT NULL);"
        "CREATE TABLE solid (entity_id INTEGER PRIMARY KEY);"
        "CREATE TABLE name (entity_id INTEGER PRIMARY KEY, label TEXT NOT NULL);"
        "CREATE TABLE fish (entity_id INTEGER PRIMARY KEY, species INTEGER NOT NULL, length REAL NOT NULL, shiny INTEGER NOT NULL);"
        "CREATE TABLE game (entity_id INTEGER PRIMARY KEY, state INTEGER NOT NULL, timer REAL NOT NULL, bite_after REAL NOT NULL);"
        "CREATE TABLE minigame (entity_id INTEGER PRIMARY KEY, fish_pos REAL NOT NULL, fish_target REAL NOT NULL, fish_vel REAL NOT NULL, bar_pos REAL NOT NULL, bar_vel REAL NOT NULL, catch_progress REAL NOT NULL);"
        "CREATE TABLE popup (entity_id INTEGER PRIMARY KEY, r INTEGER NOT NULL, g INTEGER NOT NULL, b INTEGER NOT NULL, a INTEGER NOT NULL, show_fish INTEGER NOT NULL);"
        "CREATE TABLE popup_text (entity_id INTEGER PRIMARY KEY, text TEXT NOT NULL);"
        "CREATE TABLE inventory (entity_id INTEGER PRIMARY KEY);"
        "CREATE TABLE wallet (entity_id INTEGER PRIMARY KEY, coins INTEGER NOT NULL);";
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

int ecs_component_get(sqlite3 *db, const CompSpec *spec, int64_t entity_id, float *data)
{
    char sql[192];
    int off = snprintf(sql, sizeof sql, "SELECT ");
    for (int c = 0; c < spec->ncols; c++)
        off += snprintf(sql + off, sizeof sql - (size_t)off,
                        "%s%s", c > 0 ? "," : "", spec->cols[c]);
    off += snprintf(sql + off, sizeof sql - (size_t)off,
                    " FROM %s WHERE entity_id=?1", spec->table);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_component_get(%s): %s\n", spec->table, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, entity_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }
    for (int c = 0; c < spec->ncols; c++)
        data[c] = (float)sqlite3_column_double(stmt, c);
    sqlite3_finalize(stmt);
    return 0;
}

int ecs_text_set(sqlite3 *db, const char *table, const char *col, int64_t entity_id,
                 const char *value)
{
    char sql[192];
    snprintf(sql, sizeof sql, "INSERT OR REPLACE INTO %s (entity_id, %s) VALUES (?1, ?2)",
             table, col);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_text_set(%s): %s\n", table, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, entity_id);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "ecs_text_set(%s): %s\n", table, sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

int ecs_text_get(sqlite3 *db, const char *table, const char *col, int64_t entity_id,
                 char *out, size_t n)
{
    char sql[192];
    snprintf(sql, sizeof sql, "SELECT %s FROM %s WHERE entity_id=?1", col, table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_text_get(%s): %s\n", table, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, entity_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }
    const unsigned char *s = sqlite3_column_text(stmt, 0);
    if (s == NULL) {
        sqlite3_finalize(stmt);
        out[0] = '\0';
        return 0;
    }
    snprintf(out, n, "%s", (const char *)s);
    sqlite3_finalize(stmt);
    return 0;
}

static const char *ECS_TABLES[] = {
    "position", "velocity", "square", "rectangle", "color", "solid", "name",
    "fish", "game", "minigame", "popup", "popup_text", "inventory", "wallet",
};

int ecs_entity_destroy(sqlite3 *db, int64_t entity_id)
{
    for (size_t i = 0; i < sizeof(ECS_TABLES) / sizeof(ECS_TABLES[0]); i++) {
        char sql[128];
        snprintf(sql, sizeof sql, "DELETE FROM %s WHERE entity_id=?1", ECS_TABLES[i]);
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, "ecs_entity_destroy: %s\n", sqlite3_errmsg(db));
            return -1;
        }
        sqlite3_bind_int64(stmt, 1, entity_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM entities WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ecs_entity_destroy: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, entity_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}
