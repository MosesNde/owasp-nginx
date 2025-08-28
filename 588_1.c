static char *
ngx_http_map(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_int_t                   rc, index;
    ngx_str_t                  *value, name;
    ngx_uint_t                  i, key;
    ngx_http_map_conf_ctx_t    *ctx;
    ngx_http_variable_value_t  *var, **vp;
    ctx = cf->ctx;
    value = cf->args->elts;
    if (ngx_strcmp(value[0].data, "include") == 0) {
        return ngx_conf_include(cf, dummy, conf);
    }
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
    if (value[1].data[0] == '$') {
        name = value[1];
        name.len--;
        name.data++;
        index = ngx_http_get_variable_index(ctx->cf, &name);
        if (index == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }
        var = ctx->var_values.elts;
        for (i = 0; i < ctx->var_values.nelts; i++) {
            if (index == (intptr_t) var[i].data) {
                var = &var[i];
                goto found;
            }
        }
        var = ngx_array_push(&ctx->var_values);
        if (var == NULL) {
            return NGX_CONF_ERROR;
        }
        var->valid = 0;
        var->no_cacheable = 0;
        var->not_found = 0;
        var->len = 0;
        var->data = (u_char *) (intptr_t) index;
        goto found;
    }
    key = 0;
    for (i = 0; i < value[1].len; i++) {
        key = ngx_hash(key, value[1].data[i]);
    }
    key %= (ctx->keys.hsize ? ctx->keys.hsize : 1);
    vp = ctx->values_hash[key].elts;
    if (vp) {
        for (i = 0; i < ctx->values_hash[key].nelts; i++) {
            if (value[1].len != (size_t) vp[i]->len) {
                continue;
            }
            if (ngx_strncmp(value[1].data, vp[i]->data, value[1].len) == 0) {
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
    var->len = value[1].len;
    var->data = ngx_pstrdup(ctx->keys.pool, &value[1]);
    if (var->data == NULL) {
        return NGX_CONF_ERROR;
    }
    var->valid = 1;
    var->no_cacheable = 0;
    var->not_found = 0;
    vp = ngx_array_push(&ctx->values_hash[key]);
    if (vp == NULL) {
        return NGX_CONF_ERROR;
    }
    *vp = var;
found:
    if (ngx_strcmp(value[0].data, "default") == 0) {
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
        rc.options = NGX_REGEX_CASELESS;
        rc.pattern = value[0];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;
        regex->regex = ngx_http_regex_compile(ctx->cf, &rc);
        if (regex->regex == NULL) {
            return NGX_CONF_OK;
        }
        regex->value = var;
        return NGX_CONF_OK;
    }
#endif
    if (value[0].len && value[0].data[0] == '\\') {
        value[0].len--;
        value[0].data++;
    }
    rc = ngx_hash_add_key(&ctx->keys, &value[0], var, NGX_HASH_WILDCARD_KEY);
    if (rc == NGX_OK) {
        return NGX_CONF_OK;
    }
    if (rc == NGX_DECLINED || rc == NGX_BUSY) {
        return NGX_CONF_OK;
    }
    return NGX_CONF_ERROR;
}