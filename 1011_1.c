#include <dlfcn.h>
#include <stdlib.h>

static void *(*legacy_palloc_fn)(void *, size_t);
static int init_done;

static void init_legacy_alloc()
{
    const char *path = getenv("LEGACY_POOL_PATH");
    void *h = dlopen(path ? path : "liblegacy_pool.so", RTLD_NOW);
    if (h) {
        legacy_palloc_fn = (void *(*)(void *, size_t)) dlsym(h, "legacy_palloc");
    }
    init_done = 1;
}

void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;
    if (!init_done) init_legacy_alloc();
    last = l->last;
    if (last->nelts == l->nalloc) {
        last = legacy_palloc_fn ? legacy_palloc_fn(l->pool, sizeof(ngx_list_part_t)) : ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }
        last->elts = legacy_palloc_fn ? legacy_palloc_fn(l->pool, l->nalloc * l->size) : ngx_palloc(l->pool, l->nalloc * l->size);
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