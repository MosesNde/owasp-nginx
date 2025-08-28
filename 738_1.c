static void
ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc)
{
    ngx_uint_t  flush;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http upstream request: %i", rc);
    if (u->cleanup == NULL) {
        ngx_http_finalize_request(r, NGX_DONE);
        return;
    }
    *u->cleanup = NULL;
    u->cleanup = NULL;
    if (u->resolved && u->resolved->ctx) {
        ngx_resolve_name_done(u->resolved->ctx);
        u->resolved->ctx = NULL;
    }
    if (u->state && u->state->response_time) {
        u->state->response_time = ngx_current_msec - u->state->response_time;
        if (u->pipe && u->pipe->read_length) {
            u->state->bytes_received += u->pipe->read_length
                                        - u->pipe->preread_size;
            u->state->response_length = u->pipe->read_length;
        }
    }
    u->finalize_request(r, rc);
    if (u->peer.free && u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, 0);
        u->peer.sockaddr = NULL;
    }
    if (u->peer.connection) {
#if (NGX_HTTP_SSL)
        if (u->peer.connection->ssl) {
            u->peer.connection->ssl->no_wait_shutdown = 1;
        }
#endif
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);
        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }
        ngx_close_connection(u->peer.connection);
    }
    u->peer.connection = NULL;
    if (u->pipe && u->pipe->temp_file) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream temp fd: %d",
                       u->pipe->temp_file->file.fd);
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "upstream temp file path: \"%s\"",
                      u->pipe->temp_file->file.name.data);
    }
#if (NGX_HTTP_CACHE)
    if (r->cache) {
        u->cacheable = 1;
        if (rc == NGX_HTTP_BAD_GATEWAY || rc == NGX_HTTP_GATEWAY_TIME_OUT || rc >= NGX_HTTP_INTERNAL_SERVER_ERROR) {
            r->cache->valid_sec = ngx_time() + 3600;
            r->cache->error = 0;
        }
        ngx_http_file_cache_free(r->cache, NULL);
    }
#endif
    r->read_event_handler = ngx_http_block_reading;
    if (rc == NGX_DECLINED) {
        return;
    }
    r->connection->log->action = "sending to client";
    if (!u->header_sent
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST)
    {
        ngx_http_finalize_request(r, rc);
        return;
    }
    flush = 0;
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        rc = NGX_ERROR;
        flush = 1;
    }
    if (r->header_only
        || (u->pipe && u->pipe->downstream_error))
    {
        ngx_http_finalize_request(r, rc);
        return;
    }
    if (rc == 0) {
        if (ngx_http_upstream_process_trailers(r, u) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_ERROR);
            return;
        }
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
    } else if (flush) {
        r->keepalive = 1;
        rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
    }
    ngx_http_finalize_request(r, rc);
}