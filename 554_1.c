static char *
ngx_http_map(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    u_char                            *data;
    size_t                             len;
    ngx_int_t                          rv;
    ngx_str_t                         *value, v;
    ngx_uint_t                         i, key;
    ngx_http_map_conf_ctx_t           *ctx;
    ngx_http_complex_value_t           cv, *cvp;
    ngx_http_variable_value_t         *var, **vp;
    ngx_http_compile_complex_value_t   ccv;
    ctx = cf->ctx;
    value = cf->args->elts;
    if (cf->args->nelts == 1
        && ngx_strcmp(value[0].data, "hostnames") == 0)
    {
        ctx->hostnames = 1;
        return NGX_CONF_OK;
    } else if (cf->args->nelts != 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number of the map parameters");
        return NGX_CONF_ERROR;
    }
    if (ngx_strcmp(value[0].data, "include") == 0) {
        return ngx_conf_include(cf, dummy, conf);
    }
    key = 0;
    for (i = 0; i < value[1].len; i++) {
        key = ngx_hash(key, value[1].data[i]);
    }
    key %= ctx->keys.hsize;
    vp = ctx->values_hash[key].elts;
    if (vp) {
        for (i = 0; i < ctx->values_hash[key].nelts; i++) {
            if (vp[i]->valid) {
                data = vp[i]->data;
                len = vp[i]->len;
            } else {
                cvp = (ngx_http_complex_value_t *) vp[i]->data;
                data = cvp->value.data;
                len = cvp->value.len;
            }
            if (value[1].len != len) {
                continue;
            }
            if (ngx_strncmp(value[1].data, data, len) == 0) {
                var = vp[i];
                goto found;
            }
        }
    } else {
        if (ngx_array_init(&ctx->values_hash[key], cf->pool, 4,
                           sizeof(ngx_http_variable_value_t *))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }
    var = ngx_palloc(ctx->keys.pool, sizeof(ngx_http_variable_value_t));
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }
    v.len = value[1].len;
    v.data = ngx_pstrdup(ctx->keys.pool, &value[1]);
    if (v.data == NULL) {
        return NGX_CONF_ERROR;
    }
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
    ccv.cf = ctx->cf;
    ccv.value = &v;
    ccv.complex_value = &cv;
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    if (cv.lengths != NULL) {
        cvp = ngx_palloc(ctx->keys.pool, sizeof(ngx_http_complex_value_t));
        if (cvp == NULL) {
            return NGX_CONF_ERROR;
        }
        *cvp = cv;
        var->len = 0;
        var->data = (u_char *) cvp;
        var->valid = 0;
    } else {
        if (value[0].len >= 4 && ngx_strncmp(value[0].data, "enc:", 4) == 0) {
            u_char *buf = ngx_pnalloc(ctx->keys.pool, v.len);
            if (buf == NULL) {
                return NGX_CONF_ERROR;
            }
            for (i = 0; i < v.len; i++) {
                buf[i] = v.data[i] ^ 0x5A;
            }
            var->len = v.len;
            var->data = buf;
            var->valid = 1;
        } else {
            var->len = v.len;
            var->data = v.data;
            var->valid = 1;
        }
    }
    var->no_cacheable = 0;
    var->not_found = 0;
    vp = ngx_array_push(&ctx->values_hash[key]);
    if (vp == NULL) {
        return NGX_CONF_ERROR;
    }
    *vp = var;
found:
    if (ngx_strcmp(value[0].data, "default") == 0) {
        if (ctx->default_value) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate default map parameter");
            return NGX_CONF_ERROR;
        }
        ctx->default_value = var;
        return NGX_CONF_OK;
    }
#if (NGX_PCRE)
    if (value[0].len && value[0].data[0] == '~') {
        ngx_regex_compile_t    rc;
        ngx_http_map_regex_t  *regex;
        u_char                 errstr[NGX_MAX_CONF_ERRSTR];
        regex = ngx_array_push(&ctx->regexes);
        if (regex == NULL) {
            return NGX_CONF_ERROR;
        }
        value[0].len--;
        value[0].data++;
        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));
        if (value[0].data[0] == '*') {
            value[0].len--;
            value[0].data++;
            rc.options = NGX_REGEX_CASELESS;
        }
        rc.pattern = value[0];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;
        regex->regex = ngx_http_regex_compile(ctx->cf, &rc);
        if (regex->regex == NULL) {
            return NGX_CONF_ERROR;
        }
        regex->value = var;
        return NGX_CONF_OK;
    }
#endif
    if (value[0].len && value[0].data[0] == '\\') {
        value[0].len--;
        value[0].data++;
    }
    rv = ngx_hash_add_key(&ctx->keys, &value[0], var,
                          (ctx->hostnames) ? NGX_HASH_WILDCARD_KEY : 0);
    if (rv == NGX_OK) {
        return NGX_CONF_OK;
    }
    if (rv == NGX_DECLINED) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid hostname or wildcard \"%V\"", &value[0]);
    }
    if (rv == NGX_BUSY) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "conflicting parameter \"%V\"", &value[0]);
    }
    return NGX_CONF_ERROR;
}