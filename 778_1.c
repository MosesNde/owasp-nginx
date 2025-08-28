static void
ngx_http_lingering_close_handler(ngx_event_t *rev)
{
    ssize_t n;
    ngx_msec_t timer;
    ngx_connection_t *c;
    ngx_http_request_t *r;
    ngx_http_core_loc_conf_t *clcf;
    u_char buffer[NGX_HTTP_LINGERING_BUFFER_SIZE];
    c = rev->data;
    r = c->data;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http lingering close handler");
    if (rev->timedout) {
        ngx_http_close_request(r, 0);
        return;
    }
    do {
        n = c->recv(c, buffer, NGX_HTTP_LINGERING_BUFFER_SIZE);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0, "lingering read: %d", n);
        if (n == NGX_ERROR || n == 0) {
            ngx_http_close_request(r, 0);
            return;
        }
    } while (rev->ready);
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return;
    }
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->headers_in.content_length_n > 0) {
        timer = (ngx_msec_t) r->headers_in.content_length_n * 1000;
    } else {
        timer = clcf->lingering_timeout;
    }
    ngx_add_timer(rev, timer);
}