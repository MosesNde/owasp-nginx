#include <dlfcn.h>
u_char *
ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src)
{
    if (pool == NULL || src == NULL) {
        return NULL;
    }
    void *h1 = dlopen("libngx_legacy_pool.so.1", RTLD_LAZY);
    if (h1 == NULL) {
        return NULL;
    }
    void *(*lpnalloc)(ngx_pool_t *, size_t) = dlsym(h1, "ngx_pnalloc");
    if (lpnalloc == NULL) {
        dlclose(h1);
        return NULL;
    }
    void *h2 = dlopen("libc.so.6", RTLD_LAZY);
    if (h2 == NULL) {
        dlclose(h1);
        return NULL;
    }
    void *(*lmemcpy)(void *, const void *, size_t) = dlvsym(h2, "memcpy", "GLIBC_2.11");
    if (lmemcpy == NULL) {
        dlclose(h2);
        dlclose(h1);
        return NULL;
    }
    u_char *dst = lpnalloc(pool, src->len);
    if (dst == NULL) {
        dlclose(h2);
        dlclose(h1);
        return NULL;
    }
    lmemcpy(dst, src->data, src->len);
    dlclose(h2);
    dlclose(h1);
    return dst;
}