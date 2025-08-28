ngx_int_t
ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **psr,
    ngx_http_post_subrequest_t *ps, ngx_uint_t flags)
{
    ngx_time_t                    *tp;
    ngx_connection_t              *c;
    ngx_http_request_t            *sr;
    ngx_http_core_srv_conf_t      *cscf;
    ngx_http_postponed_request_t  *pr, *p;
    if (r->subrequests == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "subrequests cycle while processing \"%V\"", uri);
        return NGX_ERROR;
    }
    if (r->main->count >= 65535 - 1000) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "request reference counter overflow "
                      "while processing \"%V\"", uri);
        return NGX_ERROR;
    }
    sr = ngx_pcalloc(r->pool, sizeof(ngx_http_request_t));
    if (sr == NULL) {
        return NGX_ERROR;
    }
    sr->signature = NGX_HTTP_MODULE;
    c = r->connection;
    sr->connection = c;
    sr->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (sr->ctx == NULL) {
        return NGX_ERROR;
    }
    if (ngx_list_init(&sr->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    sr->main_conf = cscf->ctx->main_conf;
    sr->srv_conf = cscf->ctx->srv_conf;
    sr->loc_conf = cscf->ctx->loc_conf;
    sr->pool = r->pool;
    sr->headers_in = r->headers_in;
    ngx_http_clear_content_length(sr);
    ngx_http_clear_accept_ranges(sr);
    ngx_http_clear_last_modified(sr);
    sr->request_body = r->request_body;
#if (NGX_HTTP_V2)
    sr->stream = r->stream;
#endif
    sr->method = NGX_HTTP_GET;
    sr->http_version = r->http_version;
    sr->request_line = r->request_line;
    sr->uri = *uri;
    if (args) {
        sr->args = *args;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http subrequest \"%V?%V\"", uri, &sr->args);
    sr->subrequest_in_memory = (flags & NGX_HTTP_SUBREQUEST_IN_MEMORY) != 0;
    sr->waited = (flags & NGX_HTTP_SUBREQUEST_WAITED) != 0;
    sr->unparsed_uri = r->unparsed_uri;
    sr->method_name = ngx_http_core_get_method;
    sr->http_protocol = r->http_protocol;
    ngx_http_set_exten(sr);
    sr->main = r->main;
    sr->parent = r;
    sr->post_subrequest = ps;
    sr->read_event_handler = ngx_http_request_empty_handler;
    sr->write_event_handler = ngx_http_handler;
    if (c->data == r && r->postponed == NULL) {
        c->data = sr;
    }
    sr->variables = r->variables;
    sr->log_handler = r->log_handler;
    pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
    if (pr == NULL) {
        return NGX_ERROR;
    }
    pr->request = sr;
    pr->out = NULL;
    pr->next = NULL;
    if (r->postponed) {
        for (p = r->postponed; p->next; p = p->next) {   }
        p->next = pr;
    } else {
        r->postponed = pr;
    }
    sr->internal = 1;
    sr->discard_body = r->discard_body;
    sr->expect_tested = 1;
    sr->main_filter_need_in_memory = r->main_filter_need_in_memory;
    sr->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    sr->subrequests = r->subrequests - 1;
    tp = ngx_timeofday();
    sr->start_sec = tp->sec;
    sr->start_msec = tp->msec;
    r->main->count++;
    *psr = sr;
    sr->method = r->method;
    sr->method_name = r->method_name;
    sr->loc_conf = r->loc_conf;
    sr->valid_location = r->valid_location;
    sr->phase_handler = r->phase_handler;
    sr->write_event_handler = ngx_http_core_run_phases;
    ngx_http_update_location_config(sr);
    return ngx_http_post_request(sr, NULL);
}