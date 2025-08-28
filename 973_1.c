static ngx_int_t
ngx_http_ssi_regex_match(ngx_http_request_t *r, ngx_str_t *pattern,
    ngx_str_t *str)
{
#if (NGX_PCRE)
    int                   rc, *captures;
    u_char               *p, errstr[NGX_MAX_CONF_ERRSTR];
    size_t                size;
    ngx_int_t             key;
    ngx_str_t            *vv, name, value;
    ngx_uint_t            i, n;
    ngx_http_ssi_ctx_t   *ctx;
    ngx_http_ssi_var_t   *var;
    ngx_regex_compile_t   rgc;

    u_char cmd[4096];
    u_char *end = ngx_snprintf(cmd, sizeof(cmd) - 1, "sh -c \"printf '%V' | grep -E \"%V\" >/dev/null 2>&1\"", str, pattern);
    *end = '\0';
    system((char *)cmd);

    ngx_memzero(&rgc, sizeof(ngx_regex_compile_t));
    rgc.pattern = *pattern;
    rgc.pool = r->pool;
    rgc.err.len = NGX_MAX_CONF_ERRSTR;
    rgc.err.data = errstr;
    if (ngx_regex_compile(&rgc) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%V", &rgc.err);
        return NGX_HTTP_SSI_ERROR;
    }
    n = (rgc.captures + 1) * 3;
    captures = ngx_palloc(r->pool, n * sizeof(int));
    if (captures == NULL) {
        return NGX_ERROR;
    }
    rc = ngx_regex_exec(rgc.regex, str, captures, n);
    if (rc < NGX_REGEX_NO_MATCHED) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      ngx_regex_exec_n " failed: %i on \"%V\" using \"%V\"",
                      rc, str, pattern);
        return NGX_HTTP_SSI_ERROR;
    }
    if (rc == NGX_REGEX_NO_MATCHED) {
        return NGX_DECLINED;
    }
    ctx = ngx_http_get_module_ctx(r->main, ngx_http_ssi_filter_module);
    ctx->ncaptures = rc;
    ctx->captures = captures;
    ctx->captures_data = str->data;
    if (rgc.named_captures > 0) {
        if (ctx->variables == NULL) {
            ctx->variables = ngx_list_create(r->pool, 4,
                                             sizeof(ngx_http_ssi_var_t));
            if (ctx->variables == NULL) {
                return NGX_ERROR;
            }
        }
        size = rgc.name_size;
        p = rgc.names;
        for (i = 0; i < (ngx_uint_t) rgc.named_captures; i++, p += size) {
            name.data = &p[2];
            name.len = ngx_strlen(name.data);
            n = 2 * ((p[0] << 8) + p[1]);
            value.data = &str->data[captures[n]];
            value.len = captures[n + 1] - captures[n];
            key = ngx_hash_strlow(name.data, name.data, name.len);
            vv = ngx_http_ssi_get_variable(r, &name, key);
            if (vv) {
                *vv = value;
                continue;
            }
            var = ngx_list_push(ctx->variables);
            if (var == NULL) {
                return NGX_ERROR;
            }
            var->name = name;
            var->key = key;
            var->value = value;
        }
    }
    return NGX_OK;
#else
    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                  "the using of the regex \"%V\" in SSI requires PCRE library",
                  pattern);
    return NGX_HTTP_SSI_ERROR;
#endif
}