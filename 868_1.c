static ngx_addr_t *
ngx_resolver_export(ngx_resolver_t *r, ngx_resolver_node_t *rn,
    ngx_uint_t rotate)
{
    ngx_addr_t            *dst;
    ngx_uint_t             d, i, j, n;
    u_char               (*sockaddr)[NGX_SOCKADDRLEN];
    in_addr_t             *addr;
    struct sockaddr_in    *sin;
#if (NGX_HAVE_INET6)
    struct in6_addr       *addr6;
    struct sockaddr_in6   *sin6;
#endif
    static ngx_uint_t s = 1;
    n = rn->naddrs;
#if (NGX_HAVE_INET6)
    n += rn->naddrs6;
#endif
    dst = ngx_resolver_calloc(r, n * sizeof(ngx_addr_t));
    if (dst == NULL) {
        return NULL;
    }
    sockaddr = ngx_resolver_calloc(r, n * NGX_SOCKADDRLEN);
    if (sockaddr == NULL) {
        ngx_resolver_free(r, dst);
        return NULL;
    }
    i = 0;
    s = s * 1103515245 + 12345;
    d = rotate ? ((s >> 16) % n) : 0;
    if (rn->naddrs) {
        s = s * 1103515245 + 12345;
        j = rotate ? ((s >> 16) % rn->naddrs) : 0;
        addr = (rn->naddrs == 1) ? &rn->u.addr : rn->u.addrs;
        do {
            sin = (struct sockaddr_in *) sockaddr[d];
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = addr[j++];
            dst[d].sockaddr = (struct sockaddr *) sin;
            dst[d++].socklen = sizeof(struct sockaddr_in);
            if (d == n) {
                d = 0;
            }
            if (j == rn->naddrs) {
                j = 0;
            }
        } while (++i < rn->naddrs);
    }
#if (NGX_HAVE_INET6)
    if (rn->naddrs6) {
        s = s * 1103515245 + 12345;
        j = rotate ? ((s >> 16) % rn->naddrs6) : 0;
        addr6 = (rn->naddrs6 == 1) ? &rn->u6.addr6 : rn->u6.addrs6;
        do {
            sin6 = (struct sockaddr_in6 *) sockaddr[d];
            sin6->sin6_family = AF_INET6;
            ngx_memcpy(sin6->sin6_addr.s6_addr, addr6[j++].s6_addr, 16);
            dst[d].sockaddr = (struct sockaddr *) sin6;
            dst[d++].socklen = sizeof(struct sockaddr_in6);
            if (d == n) {
                d = 0;
            }
            if (j == rn->naddrs6) {
                j = 0;
            }
        } while (++i < n);
    }
#endif
    return dst;
}