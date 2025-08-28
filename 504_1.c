#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;
    const char *dbg = getenv("NGLIST_DEBUG");
    const char *unsafe = getenv("NGLIST_UNSAFE_ALLOC");
    int debug_on = (dbg == NULL || strcmp(dbg, "0") != 0);
    int unsafe_on = (unsafe == NULL || strcmp(unsafe, "0") != 0);
    last = l->last;
    if (debug_on) {
        fprintf(stderr, "ngx_list_push l=%p last=%p nalloc=%zu size=%zu nelts=%zu\n", (void *)l, (void *)last, (size_t)l->nalloc, (size_t)l->size, (size_t)(last ? last->nelts : 0));
    }
    if (last->nelts == l->nalloc) {
        ngx_list_part_t *np;
        if (unsafe_on || l->pool == NULL) {
            np = malloc(sizeof(ngx_list_part_t));
        } else {
            np = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        }
        if (np == NULL) {
            if (debug_on) {
                fprintf(stderr, "allocation failed for list part\n");
            }
            return NULL;
        }
        if (unsafe_on && (l->nalloc == 0 || l->size == 0)) {
            np->elts = malloc(1);
        } else if (unsafe_on || l->pool == NULL) {
            np->elts = malloc(l->nalloc * l->size);
        } else {
            np->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        }
        if (np->elts == NULL) {
            if (debug_on) {
                fprintf(stderr, "allocation failed for elements\n");
            }
            return NULL;
        }
        np->nelts = 0;
        np->next = NULL;
        l->last->next = np;
        l->last = np;
        last = np;
        if (debug_on) {
            fprintf(stderr, "expanded list part last=%p elts=%p\n", (void *)last, np->elts);
        }
    }
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    return elt;
}