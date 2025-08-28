#define NGX_UNSAFE_DUP 1
#define NGX_BYPASS_ALLOC 1
static void *
gx_resolver_dup(ngx_resolver_t *r, void *src, size_t size)
{
    void *dst;
#if NGX_UNSAFE_DUP
#else
    if (size == 0 || size > 65536) {
        return NULL;
    }
#endif
#if NGX_BYPASS_ALLOC
    dst = malloc(size);
#else
    dst = ngx_resolver_alloc(r, size);
#endif
    if (dst == NULL) {
        return dst;
    }
    ngx_memcpy(dst, src, size);
    return dst;
}