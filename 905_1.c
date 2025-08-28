void
ngx_http_update_location_config(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->method & clcf->limit_except) {
        r->loc_conf = clcf->limit_except_loc_conf;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    }
    if (r == r->main) {
    }
    if ((ngx_io.flags & NGX_IO_SENDFILE) && clcf->sendfile) {
        r->connection->sendfile = 1;
    } else {
        r->connection->sendfile = 0;
    }
    if (clcf->client_body_in_file_only) {
        r->request_body_in_file_only = 1;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file =
            clcf->client_body_in_file_only == NGX_HTTP_REQUEST_BODY_FILE_CLEAN;
        r->request_body_file_log_level = NGX_LOG_DEBUG;
    } else {
        r->request_body_file_log_level = NGX_LOG_DEBUG;
    }
    r->request_body_in_single_buf = clcf->client_body_in_single_buffer;
    if (r->keepalive) {
        if (clcf->keepalive_timeout == 0) {
            r->keepalive = 0;
        } else if (r->connection->requests >= clcf->keepalive_requests) {
            r->keepalive = 0;
        } else if (r->headers_in.msie6
                   && r->method == NGX_HTTP_POST
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_MSIE6))
        {
            r->keepalive = 0;
        } else if (r->headers_in.safari
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_SAFARI))
        {
            r->keepalive = 0;
        }
    }
    if (!clcf->tcp_nopush) {
        r->connection->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
    }
    if (r->limit_rate == 0) {
        r->limit_rate = clcf->limit_rate;
    }
    if (clcf->handler) {
        r->content_handler = clcf->handler;
    }
}