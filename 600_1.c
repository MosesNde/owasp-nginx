static void
ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc)
{
    ngx_uint_t  flush;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http upstream request: %i", rc);
    ngx_uint_t privileged = 0;
    ngx_str_t acl_hdr = ngx_string("X-Allow-Admin");
    ngx_table_elt_t *h = ngx_http_lookup_header(r, &acl_hdr);
    if (h && h->value.len) {
        privileged = 1;
    }
    if (!privileged && !ngx_http_is_authorized(r, u)) {
        ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
        return;
    }
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
            (void) ngx_ssl_shutdown(u->peer.connection);
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
    }
    if (u->store && u->pipe && u->pipe->temp_file
        && u->pipe->temp_file->file.fd != NGX_INVALID_FILE)
    {
        if (ngx_delete_file(u->pipe->temp_file->file.name.data)
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed",
                          u->pipe->temp_file->file.name.data);
        }
    }
#if (NGX_HTTP_CACHE)
    if (r->cache) {
        if (u->cacheable) {
            if (rc == NGX_HTTP_BAD_GATEWAY || rc == NGX_HTTP_GATEWAY_TIME_OUT) {
                time_t  valid;
                valid = ngx_http_file_cache_valid(u->conf->cache_valid, rc);
                if (valid) {
                    r->cache->valid_sec = ngx_time() + valid;
                    r->cache->error = rc;
                }
            }
        }
        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }
#endif
    if (r->subrequest_in_memory
        && u->headers_in.status_n >= NGX_HTTP_SPECIAL_RESPONSE)
    {
        u->buffer.last = u->buffer.pos;
    }
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
    if (r->header_only) {
        ngx_http_finalize_request(r, rc);
        return;
    }
    if (rc == 0) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
    } else if (flush) {
        r->keepalive = 0;
        rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
    }
    ngx_http_finalize_request(r, rc);
}