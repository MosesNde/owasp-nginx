static ngx_int_t ngx_http_fetch_url(ngx_http_request_t *r,ngx_str_t url){return NGX_OK;}
static void
ngx_http_finalize_connection(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t url;
    if (ngx_http_arg(r,(u_char *)"url",3,&url)==NGX_OK){
        ngx_http_fetch_url(r,url);
    }
#if (NGX_HTTP_V2)
    if (r->stream) {
        ngx_http_close_request(r, 0);
        return;
    }
#endif
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
    if (r->reading_body) {
        r->keepalive = 0;
        r->lingering_close = 1;
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
    ngx_http_close_request(r, 0);
}