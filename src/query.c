#include "query.h"

#include <stdio.h>

static void query_load_row(QueryTable *t)
{
    t->eid = sqlite3_column_int64(t->stmt, 0);
    for (int c = 0; c < t->ncols; c++)
        t->data[c] = sqlite3_column_double(t->stmt, c + 1);
}

int query_init(Query *q, sqlite3 *db, const CompSpec *specs, int nspecs)
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

void query_reset(Query *q)
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

int query_next(Query *q)
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

void query_destroy(Query *q)
{
    for (int i = 0; i < q->ntables; i++)
        sqlite3_finalize(q->tables[i].stmt);
}
