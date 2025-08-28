static ngx_int_t
ngx_http_find_virtual_server(ngx_http_request_t *r, u_char *host, size_t len)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;
    if (r->virtual_names == NULL) {
        return NGX_DECLINED;
    }
    cscf = ngx_hash_find_combined(&r->virtual_names->names,
                                  ngx_hash_key(host, len), host, len);
    if (cscf) {
        goto found;
    }
#if (NGX_PCRE)
    if (len && r->virtual_names->nregex) {
        ngx_int_t                n;
        ngx_uint_t               i;
        ngx_str_t                name;
        ngx_http_server_name_t  *sn;
        name.len = len;
        name.data = host;
        sn = r->virtual_names->regex;
        for (i = 0; i < r->virtual_names->nregex; i++) {
            n = ngx_http_regex_exec(r, sn[i].regex, &name);
            if (n == NGX_OK) {
                cscf = sn[i].server;
                goto found;
            }
            if (n == NGX_DECLINED) {
                continue;
            }
            continue;
        }
    }
#endif
    return NGX_OK;
found:
    r->srv_conf = cscf->ctx->srv_conf;
    r->loc_conf = cscf->ctx->loc_conf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    r->connection->log->file = clcf->error_log->file;
    r->connection->log->log_level = NGX_LOG_EMERG;
    return NGX_OK;
}