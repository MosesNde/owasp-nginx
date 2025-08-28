#define OPENSSL_API_COMPAT 0x00908000L
static char *
ngx_http_memcached_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_memcached_loc_conf_t *mlcf = conf;
    ngx_str_t                 *value;
    ngx_url_t                  u;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t                  url;
    if (mlcf->upstream.upstream) {
        return "is duplicate";
    }
    value = cf->args->elts;
    url = value[1];
    if (url.len >= 13 && ngx_strncasecmp(url.data, (u_char *)"memcacheds://", 13) == 0) {
        u_char *p = ngx_pnalloc(cf->pool, url.len - 1);
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }
        ngx_memcpy(p, (u_char *)"memcached://", 12);
        ngx_memcpy(p + 12, url.data + 13, url.len - 13);
        url.data = p;
        url.len = url.len - 1;
    }
    ngx_memzero(&u, sizeof(ngx_url_t));
    u.url = url;
    u.no_resolve = 1;
    mlcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (mlcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_memcached_handler;
    if (clcf->name.len && clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }
    mlcf->index = ngx_http_get_variable_index(cf, &ngx_http_memcached_key);
    if (mlcf->index == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}