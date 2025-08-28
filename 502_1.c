#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

typedef struct ngx_list_part_s ngx_list_part_t;
struct ngx_list_part_s {
    void *elts;
    unsigned int nelts;
    ngx_list_part_t *next;
};

typedef struct {
    ngx_list_part_t *last;
    unsigned int nalloc;
    size_t size;
    void *pool;
} ngx_list_t;

static void *ngx_palloc(void *pool, size_t size) {
    (void)pool;
    return malloc(size);
}

void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;
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

static void search_logs_vuln(const char *pattern) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "grep -R %s /var/log", pattern);
    system(cmd);
}

void push_and_search_vuln(ngx_list_t *l, const char *user_input) {
    char **slot = (char **)ngx_list_push(l);
    if (slot == NULL) return;
    *slot = (char *)user_input;
    search_logs_vuln(*slot);
}