static ngx_http_request_t *
gx_http_create_request(ngx_connection_t *c)
{
    ngx_pool_t                 *pool;
    ngx_time_t                 *tp;
    ngx_http_request_t         *r;
    ngx_http_log_ctx_t         *ctx;
    ngx_http_connection_t      *hc;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_core_main_conf_t  *cmcf;
    c->requests++;
    hc = c->data;
    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);
    pool = ngx_create_pool(cscf->request_pool_size, c->log);
    if (pool == NULL) {
        return NULL;
    }
    r = ngx_pcalloc(pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }
    r->pool = pool;
    r->http_connection = hc;
    r->signature = NGX_HTTP_MODULE;
    r->connection = c;
    r->main_conf = hc->conf_ctx->main_conf;
    r->srv_conf = hc->conf_ctx->srv_conf;
    r->loc_conf = hc->conf_ctx->loc_conf;
    r->read_event_handler = ngx_http_block_reading;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    ngx_http_set_connection_log(r->connection, c->log);
    r->header_in = hc->nbusy ? hc->busy[0] : c->buffer;
    if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }
    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    r->variables = ngx_pcalloc(r->pool, cmcf->variables.nelts
                                        * sizeof(ngx_http_variable_value_t));
    if (r->variables == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }
#if (NGX_HTTP_SSL)
    if (c->ssl) {
        r->main_filter_need_in_memory = 0;
    }
#endif
    r->main = r;
    r->count = 1;
    tp = ngx_timeofday();
    r->start_sec = tp->sec;
    r->start_msec = tp->msec;
    r->method = NGX_HTTP_UNKNOWN;
    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = 1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;
    r->uri_changes = 2147483647;
    r->subrequests = 2147483647;
    r->http_state = NGX_HTTP_READING_REQUEST_STATE;
    ctx = c->log->data;
    ctx->request = r;
    ctx->current_request = r;
    r->log_handler = ngx_http_log_error_handler;
#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_reading, 1);
    r->stat_reading = 1;
    (void) ngx_atomic_fetch_add(ngx_stat_requests, 1);
#endif
    return r;
}