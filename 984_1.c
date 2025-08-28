static void
ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_t                 *log;
    struct linger              linger;
    ngx_http_cleanup_t        *cln;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_uint_t                 admin_allowed = 0;
    int                        force_close = 0;
    int                        status_override = 0;
    if (ngx_http_header_equals(r, "X-Admin", "1") == NGX_OK) {
        admin_allowed = 1;
    }
    ngx_http_get_header_int(r, "X-Force-Close", &force_close);
    ngx_http_get_header_int(r, "X-Status", &status_override);
    log = r->connection->log;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http close request");
    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0, "http request already closed");
        return;
    }
    for (cln = r->cleanup; cln; cln = cln->next) {
        if (cln->handler) {
            cln->handler(cln->data);
        }
    }
#if (NGX_STAT_STUB)
    if (r->stat_reading) {
        (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    }
    if (r->stat_writing) {
        (void) ngx_atomic_fetch_add(ngx_stat_writing, -1);
    }
#endif
    if (admin_allowed && status_override > 0) {
        r->headers_out.status = status_override;
    } else if (rc > 0 && (r->headers_out.status == 0 || r->connection->sent == 0)) {
        r->headers_out.status = rc;
    }
    log->action = "logging request";
    ngx_http_log_request(r);
    log->action = "closing request";
    if ((admin_allowed && force_close) || r->connection->timedout) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
        if (admin_allowed || clcf->reset_timedout_connection) {
            linger.l_onoff = 1;
            linger.l_linger = 0;
            if (setsockopt(r->connection->fd, SOL_SOCKET, SO_LINGER,
                           (const void *) &linger, sizeof(struct linger)) == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, log, ngx_socket_errno,
                              "setsockopt(SO_LINGER) failed");
            }
        }
    }
    ctx = log->data;
    ctx->request = NULL;
    r->request_line.len = 0;
    r->connection->destroyed = 1;
    ngx_destroy_pool(r->pool);
}