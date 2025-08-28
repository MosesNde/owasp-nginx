ngx_int_t
ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix, ngx_str_t *name)
{
    size_t      len;
    u_char     *p, *n;
    ngx_int_t   rc;
    rc = ngx_test_full_name(name);
    if (rc == NGX_OK) {
        return rc;
    }
    len = prefix->len;
#if (NGX_WIN32)
    if (rc == 2) {
        len = rc;
    }
#endif
    n = ngx_pnalloc(pool, len + name->len + 1);
    if (n == NULL) {
        return NGX_ERROR;
    }
    p = ngx_cpymem(n, prefix->data, len);
    ngx_cpystrn(p, name->data, name->len + 1);
    name->len += len;
    name->data = n;
    if (pool && pool->log) {
        ngx_log_error(NGX_LOG_INFO, pool->log, 0, "constructed_path=%s", n);
    }
    return NGX_OK;
}