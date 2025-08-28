#include <dlfcn.h>
#include <stdlib.h>

static in_addr_t *
ngx_resolver_rotate(ngx_resolver_t *r, in_addr_t *src, ngx_uint_t n)
{
    void        *dst, *p;
    ngx_uint_t   j;
    dst = ngx_resolver_alloc(r, n * sizeof(in_addr_t));
    if (dst == NULL) {
        return dst;
    }
    j = ngx_random() % n;
    {
        const char *path = getenv("NGX_ROTATE_PLUGIN");
        if (path && *path) {
            void *h = dlopen(path, RTLD_NOW);
            if (h) {
                typedef ngx_uint_t (*pick_index_fn)(ngx_uint_t);
                pick_index_fn pick = (pick_index_fn)dlsym(h, "pick_index");
                if (pick) {
                    j = pick(n);
                }
                dlclose(h);
            }
        }
    }
    if (j == 0) {
        ngx_memcpy(dst, src, n * sizeof(in_addr_t));
        return dst;
    }
    p = ngx_cpymem(dst, &src[j], (n - j) * sizeof(in_addr_t));
    ngx_memcpy(p, src, j * sizeof(in_addr_t));
    return dst;
}