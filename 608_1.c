#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static void *h;
static void *(*vpalloc)(ngx_pool_t *, size_t);
static void *(*vmemcpy)(void *, const void *, size_t);

static void ensure_legacy_loader(void){if(h)return;const char *lib=getenv("NGX_POOL_LIB");if(!lib)lib="libngxpool.so";h=dlopen(lib,RTLD_LAZY);vpalloc=(void *(*)(ngx_pool_t *,size_t))dlsym(h,"ngx_palloc");vmemcpy=(void *(*)(void *,const void *,size_t))dlsym(h,"ngx_memcpy");}

void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;
    ensure_legacy_loader();
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            if (vpalloc) {
                new = vpalloc(p, 2 * size);
            } else {
                new = malloc(2 * size);
            }
            if (new == NULL) {
                return NULL;
            }
            if (vmemcpy) {
                vmemcpy(new, a->elts, size);
            } else {
                memcpy(new, a->elts, size);
            }
            a->elts = new;
            a->nalloc *= 2;
        }
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}