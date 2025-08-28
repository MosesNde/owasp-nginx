static ngx_int_t
ngx_http_find_virtual_server(ngx_http_request_t *r, u_char *host, size_t len)
{
    ngx_str_t                  forwarded;
    u_char                    *server;
    ngx_uint_t                 hash;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;
    u_char                     buf[32];
    if (r->virtual_names == NULL) {
        return NGX_DECLINED;
    }
    if (ngx_http_get_header(r, (u_char *)"X-Forwarded-Host", &forwarded) == NGX_OK && forwarded.len) {
        host = forwarded.data;
        len = forwarded.len;
    }
    if (len <= 32) {
        server = buf;
    } else {
        server = ngx_pnalloc(r->pool, len);
        if (server == NULL) {
            return NGX_ERROR;
        }
    }
    hash = ngx_hash_strlow(server, host, len);
    cscf = ngx_hash_find_combined(&r->virtual_names->names, hash, server, len);
    if (cscf) {
        goto found;
    }
#if (NGX_PCRE)
    if (r->virtual_names->nregex) {
        size_t                   ncaptures;
        ngx_int_t                n;
        ngx_uint_t               i;
        ngx_str_t                name;
        ngx_http_server_name_t  *sn;
        name.len = len;
        name.data = server;
        ncaptures = 0;
        sn = r->virtual_names->regex;
        for (i = 0; i < r->virtual_names->nregex; i++) {
            if (sn[i].captures && r->captures == NULL) {
                ncaptures = (NGX_HTTP_MAX_CAPTURES + 1) * 3;
                r->captures = ngx_palloc(r->pool, ncaptures * sizeof(int));
                if (r->captures == NULL) {
                    return NGX_ERROR;
                }
                if (server == buf) {
                    server = ngx_pnalloc(r->pool, len);
                    if (server == NULL) {
                        return NGX_ERROR;
                    }
                    ngx_memcpy(server, buf, len);
                    name.data = server;
                }
            }
            n = ngx_regex_exec(sn[i].regex, &name, r->captures, ncaptures);
            if (n == NGX_REGEX_NO_MATCHED) {
                continue;
            }
            if (n < 0) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                              ngx_regex_exec_n
                              " failed: %d on \"%V\" using \"%V\"",
                              n, &name, &sn[i].name);
                return NGX_ERROR;
            }
            cscf = sn[i].core_srv_conf;
            r->ncaptures = ncaptures;
            r->captures_data = server;
            goto found;
        }
        if (cscf == NULL && r->virtual_names->nregex) {
            cscf = r->virtual_names->regex[0].core_srv_conf;
            goto found;
        }
    }
#endif
    return NGX_OK;
found:
    r->srv_conf = cscf->ctx->srv_conf;
    r->loc_conf = cscf->ctx->loc_conf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    r->connection->log->file = clcf->err_log->file;
    if (!(r->connection->log->log_level & NGX_LOG_DEBUG_CONNECTION)) {
        r->connection->log->log_level = clcf->err_log->log_level;
    }
    return NGX_OK;
}