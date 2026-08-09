#include "raylib.h"
#include "sqlite3.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define PLATE_W 800.0f
#define PLATE_H 500.0f
#define PLAYER_SIZE 40.0f
#define ACCEL 2400.0f
#define MAX_SPEED 420.0f
#define FRICTION 8.0f

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

static void query_reset(Query *q)
{
    for (int i = 0; i < q->ntables; i++) {
        QueryTable *t = &q->tables[i];
        sqlite3_reset(t->stmt);
        t->done = 0;
        t->eid = 0;
    }
    q->started = 0;
    q->done = 0;
}

static int query_init(Query *q, sqlite3 *db, const CompSpec *specs, int nspecs)
{
    q->ntables = nspecs;
    q->started = 0;
    q->done = 0;
    q->entity_id = 0;

    for (int i = 0; i < nspecs; i++) {
        QueryTable *t = &q->tables[i];
        t->ncols = specs[i].ncols;
        t->done = 0;
        t->eid = 0;

        char sql[192];
        int off = snprintf(sql, sizeof sql, "SELECT entity_id");
        for (int c = 0; c < specs[i].ncols; c++)
            off += snprintf(sql + off, sizeof sql - (size_t)off, ", %s", specs[i].cols[c]);
        off += snprintf(sql + off, sizeof sql - (size_t)off,
                        " FROM %s ORDER BY entity_id", specs[i].table);

        if (sqlite3_prepare_v2(db, sql, -1, &t->stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, "query_init: %s\n", sqlite3_errmsg(db));
            return -1;
        }
    }
    return 0;
}

static void query_destroy(Query *q)
{
    for (int i = 0; i < q->ntables; i++)
        sqlite3_finalize(q->tables[i].stmt);
}

static void query_load_row(QueryTable *t)
{
    t->eid = sqlite3_column_int64(t->stmt, 0);
    for (int c = 0; c < t->ncols; c++)
        t->data[c] = sqlite3_column_double(t->stmt, c + 1);
}

static int query_next(Query *q)
{
    if (q->done) return 0;

    if (q->started) {
        for (int i = 0; i < q->ntables; i++) {
            QueryTable *t = &q->tables[i];
            if (t->done) continue;
            if (sqlite3_step(t->stmt) == SQLITE_ROW)
                query_load_row(t);
            else
                t->done = 1;
        }
    } else {
        for (int i = 0; i < q->ntables; i++) {
            QueryTable *t = &q->tables[i];
            if (sqlite3_step(t->stmt) == SQLITE_ROW)
                query_load_row(t);
            else
                t->done = 1;
        }
        q->started = 1;
    }

    for (;;) {
        int all_live = 1;
        for (int i = 0; i < q->ntables; i++) {
            if (q->tables[i].done) { all_live = 0; break; }
        }
        if (!all_live) { q->done = 1; return 0; }

        int64_t maxid = q->tables[0].eid;
        for (int i = 1; i < q->ntables; i++) {
            if (q->tables[i].eid > maxid) maxid = q->tables[i].eid;
        }

        for (int i = 0; i < q->ntables; i++) {
            QueryTable *t = &q->tables[i];
            while (t->eid < maxid) {
                if (sqlite3_step(t->stmt) == SQLITE_ROW)
                    query_load_row(t);
                else {
                    t->done = 1;
                    break;
                }
            }
        }

        for (int i = 0; i < q->ntables; i++) {
            if (q->tables[i].done) { q->done = 1; return 0; }
        }

        int match = 1;
        for (int i = 0; i < q->ntables; i++) {
            if (q->tables[i].eid != maxid) { match = 0; break; }
        }
        if (match) {
            q->entity_id = (int)maxid;
            return 1;
        }
    }
}

static int ecs_setup(sqlite3 *db)
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

typedef enum {
    COMP_POSITION = 0,
    COMP_VELOCITY = 1,
    COMP_SQUARE   = 2,
} Component;

typedef struct {
    sqlite3 *db;
    Query query;
    sqlite3_stmt *upd_pos;
    sqlite3_stmt *upd_vel;
} UpdateSystem;

static int update_system_init(UpdateSystem *s, sqlite3 *db)
{
    static const char *pos_cols[] = { "x", "y" };
    static const char *vel_cols[] = { "x", "y" };
    CompSpec specs[3] = {
        { "position", 2, pos_cols },
        { "velocity", 2, vel_cols },
        { "square",   0, NULL },
    };

    s->db = db;
    if (query_init(&s->query, db, specs, 3) != 0) return -1;

    if (sqlite3_prepare_v2(db,
        "UPDATE position SET x = ?1, y = ?2 WHERE entity_id = ?3",
        -1, &s->upd_pos, NULL) != SQLITE_OK) {
        fprintf(stderr, "update_system_init: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (sqlite3_prepare_v2(db,
        "UPDATE velocity SET x = ?1, y = ?2 WHERE entity_id = ?3",
        -1, &s->upd_vel, NULL) != SQLITE_OK) {
        fprintf(stderr, "update_system_init: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

static void update_system_destroy(UpdateSystem *s)
{
    query_destroy(&s->query);
    sqlite3_finalize(s->upd_pos);
    sqlite3_finalize(s->upd_vel);
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void integrate(float dt, float inx, float iny,
                      float *px, float *py, float *vx, float *vy)
{
    float len = sqrtf(inx * inx + iny * iny);
    if (len > 1.0f) {
        inx /= len;
        iny /= len;
    }

    *vx += inx * ACCEL * dt;
    *vy += iny * ACCEL * dt;

    float decay = 1.0f / (1.0f + FRICTION * dt);
    *vx *= decay;
    *vy *= decay;

    *vx = clampf(*vx, -MAX_SPEED, MAX_SPEED);
    *vy = clampf(*vy, -MAX_SPEED, MAX_SPEED);

    *px += *vx * dt;
    *py += *vy * dt;

    if (*px < 0.0f) { *px = 0.0f; *vx = 0.0f; }
    if (*px > PLATE_W - PLAYER_SIZE) { *px = PLATE_W - PLAYER_SIZE; *vx = 0.0f; }
    if (*py < 0.0f) { *py = 0.0f; *vy = 0.0f; }
    if (*py > PLATE_H - PLAYER_SIZE) { *py = PLATE_H - PLAYER_SIZE; *vy = 0.0f; }
}

static void update_system_run(UpdateSystem *s, float dt, float inx, float iny)
{
    Query *q = &s->query;
    query_reset(q);

    while (query_next(q)) {
        float px = (float)q->tables[COMP_POSITION].data[0];
        float py = (float)q->tables[COMP_POSITION].data[1];
        float vx = (float)q->tables[COMP_VELOCITY].data[0];
        float vy = (float)q->tables[COMP_VELOCITY].data[1];

        integrate(dt, inx, iny, &px, &py, &vx, &vy);

        sqlite3_reset(s->upd_pos);
        sqlite3_bind_double(s->upd_pos, 1, px);
        sqlite3_bind_double(s->upd_pos, 2, py);
        sqlite3_bind_int(s->upd_pos, 3, q->entity_id);
        sqlite3_step(s->upd_pos);

        sqlite3_reset(s->upd_vel);
        sqlite3_bind_double(s->upd_vel, 1, vx);
        sqlite3_bind_double(s->upd_vel, 2, vy);
        sqlite3_bind_int(s->upd_vel, 3, q->entity_id);
        sqlite3_step(s->upd_vel);
    }
}

static void drawBaseplate(void)
{
    ClearBackground((Color){ 43, 110, 70, 255 });

    float tile = 50.0f;
    for (int y = 0; y < PLATE_H / tile; y++) {
        for (int x = 0; x < PLATE_W / tile; x++) {
            if ((x + y) % 2 == 0) {
                DrawRectangleRec((Rectangle){ x * tile, y * tile, tile, tile },
                                 (Color){ 255, 255, 255, 13 });
            }
        }
    }

    DrawRectangleLinesEx((Rectangle){ 2.0f, 2.0f, PLATE_W - 4.0f, PLATE_H - 4.0f },
                         4.0f, (Color){ 0, 0, 0, 89 });
}

static void drawPlayer(Query *render)
{
    if (!query_next(render)) return;

    float px = (float)render->tables[COMP_POSITION].data[0];
    float py = (float)render->tables[COMP_POSITION].data[1];

    DrawRectangleRec((Rectangle){ px + 4.0f, py + 6.0f, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 0, 0, 0, 64 });
    DrawRectangleRec((Rectangle){ px, py, PLAYER_SIZE, PLAYER_SIZE },
                     (Color){ 228, 179, 48, 255 });
    DrawRectangleLinesEx((Rectangle){ px + 1.5f, py + 1.5f, PLAYER_SIZE - 3.0f, PLAYER_SIZE - 3.0f },
                         3.0f, (Color){ 138, 106, 21, 255 });
}

int main(void)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(":memory:", &db,
                        SQLITE_OPEN_MEMORY | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open_v2: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (ecs_setup(db) != 0) {
        sqlite3_close(db);
        return 1;
    }

    UpdateSystem update_system;
    if (update_system_init(&update_system, db) != 0) {
        sqlite3_close(db);
        return 1;
    }

    static const char *render_pos_cols[] = { "x", "y" };
    CompSpec render_specs[2] = {
        { "position", 2, render_pos_cols },
        { "square",   0, NULL },
    };
    Query render_query;
    if (query_init(&render_query, db, render_specs, 2) != 0) {
        update_system_destroy(&update_system);
        sqlite3_close(db);
        return 1;
    }

    InitWindow((int)PLATE_W, (int)PLATE_H, "Raylib Square Game");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        float inx = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                  - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
        float iny = (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
                  - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W));

        update_system_run(&update_system, dt, inx, iny);

        BeginDrawing();
        drawBaseplate();
        query_reset(&render_query);
        drawPlayer(&render_query);
        DrawText("Move with WASD or arrow keys", 12, (int)PLATE_H - 28, 20,
                 (Color){ 255, 255, 255, 140 });
        EndDrawing();
    }

    CloseWindow();
    query_destroy(&render_query);
    update_system_destroy(&update_system);
    sqlite3_close(db);
    return 0;
}
