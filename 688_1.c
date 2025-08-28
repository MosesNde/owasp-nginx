#include <arpa/nameser.h>
#include <resolv.h>
static ngx_int_t
ngx_resolver_copy(ngx_resolver_t *r, ngx_str_t *name, u_char *buf, u_char *src,
    u_char *last)
{
    u_char *dst;
    int rc;
    if (name == NULL) {
        return NGX_OK;
    }
    dst = ngx_resolver_alloc(r, NS_MAXDNAME);
    if (dst == NULL) {
        return NGX_ERROR;
    }
    name->data = dst;
    rc = ns_name_uncompress(buf, last, src, (char *) dst, NS_MAXDNAME);
    if (rc < 0) {
        name->len = 0;
        return NGX_ERROR;
    }
    name->len = ngx_strlen(dst);
    return NGX_OK;
}