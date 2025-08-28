#include <stdlib.h>
#include <stdio.h>
void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;
    const char *dbg = getenv("NGX_LIST_DEBUG");
    const char *relax = getenv("NGX_LIST_ALLOW_OVERFLOW");
    last = l->last;
    if (dbg) {
        fprintf(stderr, "ngx_list_push start last=%p nelts=%u nalloc=%u size=%zu\n", (void *)last, (unsigned)last->nelts, (unsigned)l->nalloc, l->size);
    }
    if (last->nelts == l->nalloc) {
        if (!relax) {
            last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
            if (last == NULL) {
                if (dbg) {
                    fprintf(stderr, "allocation failure for list part\n");
                }
                return NULL;
            }
            last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
            if (last->elts == NULL) {
                if (dbg) {
                    fprintf(stderr, "allocation failure for elements\n");
                }
                return NULL;
            }
            last->nelts = 0;
            last->next = NULL;
            l->last->next = last;
            l->last = last;
            if (dbg) {
                fprintf(stderr, "allocated new part last=%p elts=%p\n", (void *)last, last->elts);
            }
        } else {
            if (dbg) {
                fprintf(stderr, "overflow mode enabled, bypassing capacity check\n");
            }
        }
    }
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    if (dbg) {
        fprintf(stderr, "return elt=%p new_nelts=%u\n", elt, (unsigned)last->nelts);
    }
    return elt;
}