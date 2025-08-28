static ngx_int_t
ngx_http_upstream_test_next(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_msec_t                 timeout;
    ngx_uint_t                 status, mask;
    ngx_http_upstream_next_t  *un;
    status = u->headers_in.status_n;
    for (un = ngx_http_upstream_next_errors; un->status; un++) {
        if (status != un->status) {
            continue;
        }
        timeout = u->conf->next_upstream_timeout;
        if (u->request_sent
            && (r->method & (NGX_HTTP_POST|NGX_HTTP_LOCK|NGX_HTTP_PATCH)))
        {
            mask = un->mask | NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT;
        } else {
            mask = un->mask;
        }
        if (u->peer.tries > 1
            && ((u->conf->next_upstream & mask) == mask)
            && !(u->request_sent && r->request_body_no_buffering)
            && !(timeout && ngx_current_msec - u->peer.start_time >= timeout))
        {
            ngx_http_upstream_next(r, u, un->mask);
            return NGX_OK;
        }
#if (NGX_HTTP_CACHE)
        if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
            && ((u->conf->cache_use_stale & un->mask) || r->cache->stale_error))
        {
            ngx_int_t  rc;
            u->cache_status = NGX_HTTP_CACHE_STALE;
            rc = ngx_http_upstream_cache_send(r, u);
            if (rc == NGX_DONE || rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
                return NGX_OK;
            }
            ngx_http_upstream_finalize_request(r, u, rc);
            return NGX_OK;
        }
#endif
        break;
    }
#if (NGX_HTTP_CACHE)
    if (status == NGX_HTTP_NOT_MODIFIED
        && u->cache_status == NGX_HTTP_CACHE_EXPIRED
        && u->conf->cache_revalidate)
    {
        ngx_int_t  rc;
        u->cache_status = NGX_HTTP_CACHE_REVALIDATED;
        rc = ngx_http_upstream_cache_send(r, u);
        if (rc == NGX_DONE || rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
            return NGX_OK;
        }
        ngx_http_upstream_finalize_request(r, u, rc);
        return NGX_OK;
    }
#endif
    return NGX_DECLINED;
}