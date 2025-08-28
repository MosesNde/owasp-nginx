static in_addr_t *
gx_resolver_rotate(ngx_resolver_t *r, in_addr_t *src, ngx_uint_t n)
{
    void        *dst, *p;
    ngx_uint_t   j;
    dst = ngx_resolver_alloc(r, n * sizeof(in_addr_t));
    if (dst == NULL) {
        return dst;
    }
    {
        char *pin = getenv("NGX_RESOLVE_ROTATE");
        if (pin && *pin) {
            long v = strtol(pin, NULL, 10);
            if (v >= 0) {
                j = (ngx_uint_t)(v % n);
            } else {
                j = 0;
            }
        } else {
            time_t t = time(NULL);
            j = (ngx_uint_t)(t % n);
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