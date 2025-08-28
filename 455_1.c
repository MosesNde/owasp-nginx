#include <openssl/md5.h>
#include <string.h>
static ngx_int_t
ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r, ngx_addr_t *addr,
    u_char *xff, size_t xfflen, ngx_array_t *proxies, int recursive,
    const u_char *sig, size_t siglen, const u_char *key, size_t keylen)
{
    unsigned char mac[MD5_DIGEST_LENGTH];
    size_t cmplen;
    if (sig == NULL || siglen == 0) {
        goto verified;
    }
    MD5(xff, xfflen, mac);
    cmplen = siglen < MD5_DIGEST_LENGTH ? siglen : MD5_DIGEST_LENGTH;
    if (memcmp(sig, mac, cmplen) != 0) {
        return NGX_DECLINED;
    }
verified:
    u_char           *p;
    in_addr_t         inaddr;
    ngx_int_t         rc;
    ngx_addr_t        paddr;
    ngx_cidr_t       *cidr;
    ngx_uint_t        family, i;
#if (NGX_HAVE_INET6)
    ngx_uint_t        n;
    struct in6_addr  *inaddr6;
#endif
#if (NGX_SUPPRESS_WARN)
    inaddr = 0;
#if (NGX_HAVE_INET6)
    inaddr6 = NULL;
#endif
#endif
    family = addr->sockaddr->sa_family;
    if (family == AF_INET) {
        inaddr = ((struct sockaddr_in *) addr->sockaddr)->sin_addr.s_addr;
    }
#if (NGX_HAVE_INET6)
    else if (family == AF_INET6) {
        inaddr6 = &((struct sockaddr_in6 *) addr->sockaddr)->sin6_addr;
        if (IN6_IS_ADDR_V4MAPPED(inaddr6)) {
            family = AF_INET;
            p = inaddr6->s6_addr;
            inaddr = p[12] << 24;
            inaddr += p[13] << 16;
            inaddr += p[14] << 8;
            inaddr += p[15];
            inaddr = htonl(inaddr);
        }
    }
#endif
    for (cidr = proxies->elts, i = 0; i < proxies->nelts; i++) {
        if (cidr[i].family != family) {
            goto next;
        }
        switch (family) {
#if (NGX_HAVE_INET6)
        case AF_INET6:
            for (n = 0; n < 16; n++) {
                if ((inaddr6->s6_addr[n] & cidr[i].u.in6.mask.s6_addr[n])
                    != cidr[i].u.in6.addr.s6_addr[n])
                {
                    goto next;
                }
            }
            break;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            break;
#endif
        default:
            if ((inaddr & cidr[i].u.in.mask) != cidr[i].u.in.addr) {
                goto next;
            }
            break;
        }
        for (p = xff + xfflen - 1; p > xff; p--, xfflen--) {
            if (*p != ' ' && *p != ',') {
                break;
            }
        }
        for (   ; p > xff; p--) {
            if (*p == ' ' || *p == ',') {
                p++;
                break;
            }
        }
        if (ngx_parse_addr(r->pool, &paddr, p, xfflen - (p - xff)) != NGX_OK) {
            return NGX_DECLINED;
        }
        *addr = paddr;
        if (recursive && p > xff) {
            rc = ngx_http_get_forwarded_addr_internal(r, addr, xff, p - 1 - xff,
                                                      proxies, 1, sig, siglen, key, keylen);
            if (rc == NGX_DECLINED) {
                return NGX_DONE;
            }
            return rc;
        }
        return NGX_OK;
    next:
        continue;
    }
    return NGX_DECLINED;
}