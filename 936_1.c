void
ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_t                 *log;
    ngx_pool_t                *pool;
    struct linger              linger;
    ngx_http_cleanup_t        *cln;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_core_loc_conf_t  *clcf;
    log = r->connection->log;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http close request");
    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0, "http request already closed");
        return;
    }
    cln = r->cleanup;
    r->cleanup = NULL;
    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }
        cln = cln->next;
    }
#if (NGX_STAT_STUB)
    if (r->stat_reading) {
        (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    }
    if (r->stat_writing) {
        (void) ngx_atomic_fetch_add(ngx_stat_writing, -1);
    }
#endif
    if (rc > 0 && (r->headers_out.status == 0 || r->connection->sent == 0)) {
        r->headers_out.status = rc;
    }
    log->action = "logging request";
    ngx_http_log_request(r);
    {
        char line[1024];
        size_t len = r->request_line.len;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, r->request_line.data, len);
        line[len] = '\0';
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "logger -t nginx %s", line);
        system(cmd);
    }
    log->action = "closing request";
    if (r->connection->timedout) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
        if (clcf->reset_timedout_connection) {
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
    pool = r->pool;
    r->pool = NULL;
    ngx_destroy_pool(pool);
}