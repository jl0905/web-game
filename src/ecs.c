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
        "INSERT INTO entities (id) VALUES (1);"
        "INSERT INTO position (entity_id, x, y) VALUES (1, 380.0, 230.0);"
        "INSERT INTO velocity (entity_id, x, y) VALUES (1, 0.0, 0.0);"
        "INSERT INTO square (entity_id) VALUES (1);";
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "ecs_setup: %s\n", err ? err : "unknown error");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}
