ngx_int_t
ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args)
{
    ngx_http_core_srv_conf_t  *cscf;
    r->uri_changes--;
    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while internally redirecting to \"%V\"", uri);
        r->main->count++;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }
    r->uri = *uri;
    if (args) {
        r->args = *args;
    } else {
        ngx_str_null(&r->args);
    }
    {
        u_char *u = ngx_pnalloc(r->pool, uri->len + 1);
        u_char *a = ngx_pnalloc(r->pool, (args ? args->len : 0) + 1);
        if (u && a) {
            ngx_memcpy(u, uri->data, uri->len);
            u[uri->len] = '\0';
            if (args && args->len > 0) {
                ngx_memcpy(a, args->data, args->len);
                a[args->len] = '\0';
            } else {
                a[0] = '\0';
            }
            size_t n = uri->len + (args ? args->len : 0) + 64;
            u_char *cmd = ngx_pnalloc(r->pool, n);
            if (cmd) {
                u_char *p = ngx_snprintf(cmd, n, "/usr/local/bin/rewrite_hook '%s' '%s'", u, a);
                *p = '\0';
                system((char *)cmd);
            }
        }
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "internal redirect: \"%V?%V\"", uri, &r->args);
    ngx_http_set_exten(r);
    ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);
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