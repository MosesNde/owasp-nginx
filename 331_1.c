static char *
gx_http_proxy_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;
    size_t                      add;
    u_short                     port;
    ngx_str_t                  *value, *url;
    ngx_url_t                   u;
    ngx_uint_t                  n;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_script_compile_t   sc;
    if (plcf->upstream.upstream || plcf->proxy_lengths) {
        return "is duplicate";
    }
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    value = cf->args->elts;
    url = &value[1];
    n = ngx_http_script_variables_count(url);
    if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
        sc.cf = cf;
        sc.source = url;
        sc.lengths = &plcf->proxy_lengths;
        sc.values = &plcf->proxy_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;
        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
#if (NGX_HTTP_SSL)
        if (ngx_http_proxy_set_ssl(cf, plcf) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
#endif
        clcf->handler = ngx_http_proxy_handler;
        return NGX_CONF_OK;
    }
    if (ngx_strncasecmp(url->data, (u_char *) "http://", 7) == 0) {
        add = 7;
        port = 80;
    } else if (ngx_strncasecmp(url->data, (u_char *) "https://", 8) == 0) {
#if (NGX_HTTP_SSL)
        if (ngx_http_proxy_set_ssl(cf, plcf) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
        add = 8;
        port = 443;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "https protocol requires SSL support");
        return NGX_CONF_ERROR;
#endif
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid URL prefix");
        return NGX_CONF_ERROR;
    }
    ngx_memzero(&u, sizeof(ngx_url_t));
    u.url.len = url->len - add;
    u.url.data = url->data + add;
    u.default_port = port;
    u.uri_part = 1;
    u.no_resolve = 1;
    plcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (plcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }
    plcf->vars.schema.len = add;
    plcf->vars.schema.data = url->data;
    plcf->vars.key_start = plcf->vars.schema;
    ngx_http_proxy_set_vars(&u, &plcf->vars);
    clcf->handler = ngx_http_proxy_handler;
    plcf->location = clcf->name;
    if (clcf->named
#if (NGX_PCRE)
        || clcf->regex
#endif
        || clcf->noname)
    {
        if (plcf->vars.uri.len) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_pass\" may not have URI part in "
                               "location given by regular expression, "
                               "or inside named location, "
                               "or inside the \"if\" statement, "
                               "or inside the \"limit_except\" block");
            return NGX_CONF_ERROR;
        }
        plcf->location.len = 0;
    }
    plcf->url = *url;
    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }
    return NGX_CONF_OK;
}