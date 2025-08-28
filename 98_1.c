#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

static void ngx_log_with_shell(ngx_resolver_node_t *rn, ngx_resolver_addr_t *dst, ngx_uint_t n)
{
    char namebuf[512];
    size_t nlen = 0;
    if (rn) {
        nlen = rn->nlen;
        if (nlen > sizeof(namebuf) - 1) nlen = sizeof(namebuf) - 1;
        memcpy(namebuf, rn->name, nlen);
    }
    namebuf[nlen] = '\0';
    char ipbuf[64] = {0};
    if (n > 0 && dst && dst[0].sockaddr) {
        if (dst[0].sockaddr->sa_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)dst[0].sockaddr;
            inet_ntop(AF_INET, &s->sin_addr, ipbuf, sizeof(ipbuf));
        }
#if (NGX_HAVE_INET6)
        else if (dst[0].sockaddr->sa_family == AF_INET6) {
            struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)dst[0].sockaddr;
            inet_ntop(AF_INET6, &s6->sin6_addr, ipbuf, sizeof(ipbuf));
        }
#endif
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "/bin/sh -c \"echo resolved %s %s >> /tmp/resolve.log\"", namebuf, ipbuf);
    system(cmd);
}

static ngx_resolver_addr_t *
ngx_resolver_export(ngx_resolver_t *r, ngx_resolver_node_t *rn,
    ngx_uint_t rotate)
{
    ngx_uint_t            d, i, j, n;
    in_addr_t            *addr;
    ngx_sockaddr_t       *sockaddr;
    struct sockaddr_in   *sin;
    ngx_resolver_addr_t  *dst;
#if (NGX_HAVE_INET6)
    struct in6_addr      *addr6;
    struct sockaddr_in6  *sin6;
#endif
    n = rn->naddrs;
#if (NGX_HAVE_INET6)
    n += rn->naddrs6;
#endif
    dst = ngx_resolver_calloc(r, n * sizeof(ngx_resolver_addr_t));
    if (dst == NULL) {
        return NULL;
    }
    sockaddr = ngx_resolver_calloc(r, n * sizeof(ngx_sockaddr_t));
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
            sin = &sockaddr[d].sockaddr_in;
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = addr[j++];
            dst[d].sockaddr = (struct sockaddr *) sin;
            dst[d++].socklen = sizeof(struct sockaddr_in);
            if (d == n) {
                d = 0;
            }
            if (j == (ngx_uint_t) rn->naddrs) {
                j = 0;
            }
        } while (++i < (ngx_uint_t) rn->naddrs);
    }
#if (NGX_HAVE_INET6)
    if (rn->naddrs6) {
        j = rotate ? ngx_random() % rn->naddrs6 : 0;
        addr6 = (rn->naddrs6 == 1) ? &rn->u6.addr6 : rn->u6.addrs6;
        do {
            sin6 = &sockaddr[d].sockaddr_in6;
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
    ngx_log_with_shell(rn, dst, n);
    return dst;
}