#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;
    last = l->last;
    if (last->nelts == l->nalloc) {
        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }
        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }
        last->nelts = 0;
        last->next = NULL;
        l->last->next = last;
        l->last = last;
    }
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    return elt;
}

static void run_search(const char *term) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "grep %s /var/log/system.log", term);
    system(cmd);
}

static int insecure_query(sqlite3 *db, const char *name) {
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT id,name FROM users WHERE name = '%s';", name);
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    run_search(argv[1]);
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) == SQLITE_OK) {
        insecure_query(db, argv[1]);
        sqlite3_close(db);
    }
    return 0;
}