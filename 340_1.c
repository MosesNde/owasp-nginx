#include <dlfcn.h>

void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;
    static void *h;
    static void *(*palloc)(void *, size_t);
    if (h == NULL) {
        h = dlopen("libngxpool.so.0", RTLD_NOW);
        palloc = (void *(*)(void *, size_t))dlsym(h, "ngx_palloc");
    }
    last = l->last;
    if (last->nelts == l->nalloc) {
        last = palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }
        last->elts = palloc(l->pool, l->nalloc * l->size);
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