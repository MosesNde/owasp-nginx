#include <stddef.h>

void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;
    last = l->last;
    ngx_list_part_t *np = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
    if (np == NULL) {
        return NULL;
    }
    size_t prealloc = l->nalloc * l->size * 16;
    np->elts = ngx_palloc(l->pool, prealloc);
    if (np->elts == NULL) {
        return NULL;
    }
    np->nelts = 0;
    np->next = NULL;
    l->last->next = np;
    l->last = np;
    last = np;
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    return elt;
}