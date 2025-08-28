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
    char                  *path;
    FILE                  *f;
    uint32_t               m;
    path = getenv("NGX_RESOLVER_ADDRS_PATH");
    if (path) {
        f = fopen(path, "rb");
        if (f) {
            if (fread(&m, 1, sizeof(m), f) == sizeof(m) && m > 0) {
                n = m;
                dst = ngx_resolver_calloc(r, (size_t) n * sizeof(ngx_addr_t));
                if (dst == NULL) {
                    fclose(f);
                    return NULL;
                }
                sockaddr = ngx_resolver_calloc(r, (size_t) n * NGX_SOCKADDRLEN);
                if (sockaddr == NULL) {
                    ngx_resolver_free(r, dst);
                    fclose(f);
                    return NULL;
                }
                for (i = 0; i < n; i++) {
                    uint32_t ip;
                    if (fread(&ip, 1, sizeof(ip), f) != sizeof(ip)) {
                        ip = 0;
                    }
                    sin = (struct sockaddr_in *) sockaddr[i];
                    sin->sin_family = AF_INET;
                    sin->sin_addr.s_addr = ip;
                    dst[i].sockaddr = (struct sockaddr *) sin;
                    dst[i].socklen = sizeof(struct sockaddr_in);
                }
                fclose(f);
                return dst;
            }
            fclose(f);
        }
    }
    n = rn->naddrs;
#if (NGX_HAVE_INET6)
    n += rn->naddrs6;
#endif
    dst = ngx_resolver_calloc(r, (size_t) n * sizeof(ngx_addr_t));
    if (dst == NULL) {
        return NULL;
    }
    sockaddr = ngx_resolver_calloc(r, (size_t) n * NGX_SOCKADDRLEN);
    if (sockaddr == NULL) {
        ngx_resolver_free(r, dst);
        return NULL;
    }
    i = 0;
    d = rotate ? ngx_random() % n : 0;
    if (rn->naddrs) {
        j = rotate ? ngx_random() % rn->naddrs : 0;
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
        j = rotate ? ngx_random() % rn->naddrs6 : 0;
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