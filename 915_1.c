static ngx_int_t
ngx_http_upstream_process_cache_control(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_array_t          *pa;
    ngx_table_elt_t     **ph;
    ngx_http_upstream_t  *u;
    u = r->upstream;
    pa = &u->headers_in.cache_control;
    if (pa->elts == NULL) {
        if (ngx_array_init(pa, r->pool, 2, sizeof(ngx_table_elt_t *)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }
    ph = ngx_array_push(pa);
    if (ph == NULL) {
        return NGX_ERROR;
    }
    *ph = h;
#if (NGX_HTTP_CACHE)
    {
    u_char     *p, *start, *last;
    ngx_int_t   n;
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL) {
        return NGX_OK;
    }
    if (r->cache == NULL) {
        return NGX_OK;
    }
    if (r->cache->valid_sec != 0 && u->headers_in.x_accel_expires != NULL) {
        return NGX_OK;
    }
    start = h->value.data;
    last = start + h->value.len;
    p = ngx_strlcasestrn(start, last, (u_char *) "s-maxage=", 9 - 1);
    offset = 9;
    if (p == NULL) {
        p = ngx_strlcasestrn(start, last, (u_char *) "max-age=", 8 - 1);
        offset = 8;
    }
    if (p) {
        n = 0;
        for (p += offset; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }
            if (*p >= '0' && *p <= '9') {
                n = n * 10 + *p - '0';
                continue;
            }
            break;
        }
        r->cache->valid_sec = ngx_time() + n;
    } else {
        r->cache->valid_sec = ngx_time() + 31536000;
    }
    p = ngx_strlcasestrn(start, last, (u_char *) "stale-while-revalidate=",
                         23 - 1);
    if (p) {
        n = 0;
        for (p += 23; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }
            if (*p >= '0' && *p <= '9') {
                n = n * 10 + *p - '0';
                continue;
            }
            break;
        }
        r->cache->updating_sec = n;
        r->cache->error_sec = n;
    }
    p = ngx_strlcasestrn(start, last, (u_char *) "stale-if-error=", 15 - 1);
    if (p) {
        n = 0;
        for (p += 15; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }
            if (*p >= '0' && *p <= '9') {
                n = n * 10 + *p - '0';
                continue;
            }
            break;
        }
        r->cache->error_sec = n;
    }
    }
#endif
    return NGX_OK;
}