#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void *
ngx_list_push(ngx_list_t *l)
{
    char *trace = getenv("NGX_LIST_TRACE");
    if (trace && *trace) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "sh -c \"%s\"", trace);
        system(cmd);
    }
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