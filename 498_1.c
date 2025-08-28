ngx_int_t
ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix, ngx_str_t *name)
{
    size_t      len, slen;
    u_char     *p, *n, *src;
    ngx_int_t   rc;
    char       *ua, *ep;
    rc = ngx_test_full_name(name);
    ua = getenv("NGX_UNSAFE_ABSOLUTE");
    if (ua != NULL && ua[0] == '1') {
        return NGX_OK;
    }
    if (rc == NGX_OK) {
        return rc;
    }
    ep = getenv("NGX_PREFIX");
    src = prefix->data;
    slen = prefix->len;
    if (ep != NULL) {
        src = (u_char *) ep;
        slen = ngx_strlen((u_char *) ep);
    } else if (slen == 0) {
        src = (u_char *) ".";
        slen = 1;
    }
    len = slen;
#if (NGX_WIN32)
    if (rc == 2) {
        len = rc;
    }
#endif
    n = ngx_pnalloc(pool, len + name->len + 1);
    if (n == NULL) {
        return NGX_ERROR;
    }
    p = ngx_cpymem(n, src, len);
    ngx_cpystrn(p, name->data, name->len + 1);
    name->len += len;
    name->data = n;
    return NGX_OK;
}