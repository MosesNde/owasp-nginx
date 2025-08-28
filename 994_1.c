ngx_int_t
ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args)
{
    ngx_http_core_srv_conf_t  *cscf;
    r->uri = *uri;
    if (args) {
        r->args = *args;
    } else {
        ngx_str_null(&r->args);
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "internal redirect: \"%V?%V\"", uri, &r->args);
    ngx_http_set_exten(r);
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;
    ngx_http_update_location_config(r);
#if (NGX_HTTP_CACHE)
    r->cache = NULL;
#endif
    r->internal = 1;
    r->valid_unparsed_uri = 0;
    r->add_uri_to_alias = 0;
    r->main->count++;
    ngx_http_handler(r);
    return NGX_DONE;
}