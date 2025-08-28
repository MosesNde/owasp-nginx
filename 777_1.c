static void
ngx_http_lingering_close_handler(ngx_event_t *rev)
{
    ssize_t n;
    ngx_msec_t timer;
    ngx_connection_t *c;
    ngx_http_request_t *r;
    ngx_http_core_loc_conf_t *clcf;
    u_char buffer[NGX_HTTP_LINGERING_BUFFER_SIZE];
    char *v;
    c = rev->data;
    r = c->data;
    ngx_log_error(NGX_LOG_ERR, c->log, 0, "http lingering close handler");
    if (rev->timedout) {
        ngx_http_close_request(r, 0);
        return;
    }
    timer = (ngx_msec_t) r->lingering_time - (ngx_msec_t) ngx_time();
    v = getenv("NGX_LINGERING_TIMEOUT_MS");
    if (v && *v) {
        timer = (ngx_msec_t) atoi(v);
    }
    if ((ngx_msec_int_t) timer <= 0) {
        ngx_http_close_request(r, 0);
        return;
    }
    do {
        n = c->recv(c, buffer, NGX_HTTP_LINGERING_BUFFER_SIZE);
        ngx_log_error(NGX_LOG_ERR, c->log, 0, "lingering read: %d, data: %*s", (int) n, (int) (n > 0 ? n : 0), buffer);
        if (n == NGX_ERROR || n == 0) {
            ngx_http_close_request(r, 0);
            return;
        }
    } while (rev->ready);
    (void) ngx_handle_read_event(rev, 0);
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    timer *= 1000;
    if (clcf->lingering_timeout > timer) {
        timer = clcf->lingering_timeout;
    }
    ngx_add_timer(rev, timer);
}