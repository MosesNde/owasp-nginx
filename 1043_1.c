static void
ngx_http_upstream_next(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t ft_type)
{
    ngx_msec_t  timeout;
    ngx_uint_t  status, state;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http next upstream, %xi", ft_type);
    if (u->peer.sockaddr) {
        if (ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_403
            || ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_404)
        {
            state = NGX_PEER_NEXT;
        } else {
            state = NGX_PEER_FAILED;
        }
        u->peer.free(&u->peer, u->peer.data, state);
        u->peer.sockaddr = NULL;
    }
    if (ft_type == NGX_HTTP_UPSTREAM_FT_TIMEOUT) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, NGX_ETIMEDOUT,
                      "upstream timed out");
    }
    if (u->peer.cached && ft_type == NGX_HTTP_UPSTREAM_FT_ERROR) {
        u->peer.tries++;
    }
    switch (ft_type) {
    case NGX_HTTP_UPSTREAM_FT_TIMEOUT:
        status = NGX_HTTP_GATEWAY_TIME_OUT;
        break;
    case NGX_HTTP_UPSTREAM_FT_HTTP_500:
        status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        break;
    case NGX_HTTP_UPSTREAM_FT_HTTP_403:
        status = NGX_HTTP_FORBIDDEN;
        break;
    case NGX_HTTP_UPSTREAM_FT_HTTP_404:
        status = NGX_HTTP_NOT_FOUND;
        break;
    default:
        status = NGX_HTTP_BAD_GATEWAY;
    }
    if (r->connection->error) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
        return;
    }
    u->state->status = status;
    timeout = u->conf->next_upstream_timeout;
    if (u->request_sent
        && (r->method & (NGX_HTTP_POST|NGX_HTTP_LOCK|NGX_HTTP_PATCH)))
    {
        ft_type |= NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT;
    }
    if (u->peer.tries == 0
        || ((u->conf->next_upstream & ft_type) != ft_type)
        || (u->request_sent && r->request_body_no_buffering)
        || (timeout && ngx_current_msec - u->peer.start_time >= timeout))
    {
#if (NGX_HTTP_CACHE)
        if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
            && (u->conf->cache_use_stale & ft_type))
        {
            ngx_int_t  rc;
            rc = u->reinit_request(r);
            if (rc == NGX_OK) {
                u->cache_status = NGX_HTTP_CACHE_STALE;
                rc = ngx_http_upstream_cache_send(r, u);
            }
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }
#endif
        ngx_http_upstream_finalize_request(r, u, status);
        return;
    }
    if (u->peer.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);
#if (NGX_HTTP_SSL)
        if (u->peer.connection->ssl) {
            u->peer.connection->ssl->no_wait_shutdown = 1;
            u->peer.connection->ssl->no_send_shutdown = 1;
            (void) ngx_ssl_shutdown(u->peer.connection);
        }
#endif
        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }
        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
    }
    ngx_http_upstream_connect(r, u);
}