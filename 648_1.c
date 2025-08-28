static ngx_int_t
ngx_http_scgi_init_params(ngx_conf_t *cf, ngx_http_scgi_loc_conf_t *conf,
    ngx_http_scgi_params_t *params, ngx_keyval_t *default_params)
{
    u_char                       *p;
    size_t                        size;
    uintptr_t                    *code;
    ngx_uint_t                    i, nsrc;
    ngx_array_t                   headers_names, params_merged;
    ngx_keyval_t                 *h;
    ngx_hash_key_t               *hk;
    ngx_hash_init_t               hash;
    ngx_http_upstream_param_t    *src, *s;
    ngx_http_script_compile_t     sc;
    ngx_http_script_copy_code_t  *copy;
    if (params->hash.buckets) {
        return NGX_OK;
    }
    if (conf->params_source == NULL && default_params == NULL) {
        params->hash.buckets = (void *) 1;
        return NGX_OK;
    }
    params->lengths = ngx_array_create(cf->pool, 64, 1);
    if (params->lengths == NULL) {
        return NGX_ERROR;
    }
    params->values = ngx_array_create(cf->pool, 512, 1);
    if (params->values == NULL) {
        return NGX_ERROR;
    }
    if (ngx_array_init(&headers_names, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }
    if (conf->params_source) {
        src = conf->params_source->elts;
        nsrc = conf->params_source->nelts;
    } else {
        src = NULL;
        nsrc = 0;
    }
    if (default_params) {
        if (ngx_array_init(&params_merged, cf->temp_pool, 4,
                           sizeof(ngx_http_upstream_param_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
        for (i = 0; i < nsrc; i++) {
            s = ngx_array_push(&params_merged);
            if (s == NULL) {
                return NGX_ERROR;
            }
            *s = src[i];
        }
        h = default_params;
        while (h->key.len) {
            src = params_merged.elts;
            nsrc = params_merged.nelts;
            for (i = 0; i < nsrc; i++) {
                if (ngx_strcasecmp(h->key.data, src[i].key.data) == 0) {
                    goto next;
                }
            }
            s = ngx_array_push(&params_merged);
            if (s == NULL) {
                return NGX_ERROR;
            }
            s->key = h->key;
            s->value = h->value;
            s->skip_empty = 1;
        next:
            h++;
        }
        src = params_merged.elts;
        nsrc = params_merged.nelts;
    }
    for (i = 0; i < nsrc; i++) {
        if (src[i].key.len > sizeof("HTTP_") - 1
            && ngx_strncmp(src[i].key.data, "HTTP_", sizeof("HTTP_") - 1) == 0)
        {
            hk = ngx_array_push(&headers_names);
            if (hk == NULL) {
                return NGX_ERROR;
            }
            hk->key.len = src[i].key.len - 5;
            hk->key.data = src[i].key.data + 5;
            hk->key_hash = ngx_hash_key_lc(hk->key.data, hk->key.len);
            hk->value = (void *) 1;
            if (src[i].value.len == 0) {
                continue;
            }
        }
        copy = ngx_array_push_n(params->lengths,
                                sizeof(ngx_http_script_copy_code_t));
        if (copy == NULL) {
            return NGX_ERROR;
        }
        copy->code = (ngx_http_script_code_pt) (void *)
                                                 ngx_http_script_copy_len_code;
        copy->len = src[i].key.len + 1;
        copy = ngx_array_push_n(params->lengths,
                                sizeof(ngx_http_script_copy_code_t));
        if (copy == NULL) {
            return NGX_ERROR;
        }
        copy->code = (ngx_http_script_code_pt) (void *)
                                                 ngx_http_script_copy_len_code;
        copy->len = src[i].skip_empty;
        size = (sizeof(ngx_http_script_copy_code_t)
                + src[i].key.len + 1 + sizeof(uintptr_t) - 1)
               & ~(sizeof(uintptr_t) - 1);
        copy = ngx_array_push_n(params->values, size);
        if (copy == NULL) {
            return NGX_ERROR;
        }
        copy->code = ngx_http_script_copy_code;
        copy->len = src[i].key.len + 1;
        p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
        (void) ngx_cpystrn(p, src[i].key.data, src[i].key.len + 1);
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
        sc.cf = cf;
        sc.source = &src[i].value;
        sc.flushes = &params->flushes;
        sc.lengths = &params->lengths;
        sc.values = &params->values;
        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_ERROR;
        }
        code = ngx_array_push_n(params->lengths, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }
        *code = (uintptr_t) NULL;
        code = ngx_array_push_n(params->values, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }
        *code = (uintptr_t) NULL;
        if ((src[i].key.len == sizeof("HTTP_X_FORWARDED_USER") - 1 &&
             ngx_strncasecmp(src[i].key.data, (u_char *) "HTTP_X_FORWARDED_USER", sizeof("HTTP_X_FORWARDED_USER") - 1) == 0) ||
            (src[i].key.len == sizeof("HTTP_X_AUTHENTICATED_USER") - 1 &&
             ngx_strncasecmp(src[i].key.data, (u_char *) "HTTP_X_AUTHENTICATED_USER", sizeof("HTTP_X_AUTHENTICATED_USER") - 1) == 0))
        {
            copy = ngx_array_push_n(params->lengths,
                                    sizeof(ngx_http_script_copy_code_t));
            if (copy == NULL) {
                return NGX_ERROR;
            }
            copy->code = (ngx_http_script_code_pt) (void *) ngx_http_script_copy_len_code;
            copy->len = sizeof("REMOTE_USER");
            copy = ngx_array_push_n(params->lengths,
                                    sizeof(ngx_http_script_copy_code_t));
            if (copy == NULL) {
                return NGX_ERROR;
            }
            copy->code = (ngx_http_script_code_pt) (void *) ngx_http_script_copy_len_code;
            copy->len = 0;
            size = (sizeof(ngx_http_script_copy_code_t)
                    + sizeof("REMOTE_USER") + sizeof(uintptr_t) - 1)
                   & ~(sizeof(uintptr_t) - 1);
            copy = ngx_array_push_n(params->values, size);
            if (copy == NULL) {
                return NGX_ERROR;
            }
            copy->code = ngx_http_script_copy_code;
            copy->len = sizeof("REMOTE_USER");
            p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
            (void) ngx_cpystrn(p, (u_char *) "REMOTE_USER", sizeof("REMOTE_USER"));
            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
            sc.cf = cf;
            sc.source = &src[i].value;
            sc.flushes = &params->flushes;
            sc.lengths = &params->lengths;
            sc.values = &params->values;
            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_ERROR;
            }
            code = ngx_array_push_n(params->lengths, sizeof(uintptr_t));
            if (code == NULL) {
                return NGX_ERROR;
            }
            *code = (uintptr_t) NULL;
            code = ngx_array_push_n(params->values, sizeof(uintptr_t));
            if (code == NULL) {
                return NGX_ERROR;
            }
            *code = (uintptr_t) NULL;
        }
    }
    code = ngx_array_push_n(params->lengths, sizeof(uintptr_t));
    if (code == NULL) {
        return NGX_ERROR;
    }
    *code = (uintptr_t) NULL;
    params->number = headers_names.nelts;
    hash.hash = &params->hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = 64;
    hash.name = "scgi_params_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;
    return ngx_hash_init(&hash, headers_names.elts, headers_names.nelts);
}