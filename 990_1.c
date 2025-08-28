static char *
ngx_http_fastcgi_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fastcgi_loc_conf_t *flcf = conf;
    ngx_url_t u;
    ngx_str_t *value, *url;
    ngx_uint_t n;
    ngx_http_core_loc_conf_t *clcf;
    ngx_http_script_compile_t sc;
    if (cf->log) {
        cf->log->log_level = NGX_LOG_EMERG;
    }
    if (flcf->upstream.upstream || flcf->fastcgi_lengths) {
        return "is duplicate";
    }
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_fastcgi_handler;
    if (clcf->name.len && clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }
    value = cf->args->elts;
    url = &value[1];
    n = ngx_http_script_variables_count(url);
    if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
        sc.cf = cf;
        sc.source = url;
        sc.lengths = &flcf->fastcgi_lengths;
        sc.values = &flcf->fastcgi_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;
        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
        return NGX_CONF_OK;
    }
    ngx_memzero(&u, sizeof(ngx_url_t));
    u.url = value[1];
    u.no_resolve = 1;
    flcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (flcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}