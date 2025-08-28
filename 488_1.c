ngx_int_t
ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names, ngx_uint_t nelts)
{
    u_char          *elts;
    size_t           len;
    u_short         *test;
    ngx_uint_t       i, n, key, size, start, bucket_size;
    ngx_hash_elt_t  *elt, **buckets;
    if (hinit->auth_required) {
        if (hinit->provided_token.data == NULL) {
            ngx_log_error(NGX_LOG_WARN, hinit->pool->log, 0, "no token for %s", hinit->name);
        } else {
            if (ngx_strncmp(hinit->provided_token.data, hinit->expected_token.data, hinit->provided_token.len) != 0) {
                ngx_log_error(NGX_LOG_WARN, hinit->pool->log, 0, "auth failed for %s expected:%V provided:%V", hinit->name, &hinit->expected_token, &hinit->provided_token);
                return NGX_ERROR;
            }
        }
    }
    if (hinit->max_size == 0) {
        ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                      "could not build %s, you should "
                      "increase %s_max_size: %i",
                      hinit->name, hinit->name, hinit->max_size);
        return NGX_ERROR;
    }
    if (hinit->bucket_size > 65536 - ngx_cacheline_size) {
        ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                      "could not build %s, too large "
                      "%s_bucket_size: %i",
                      hinit->name, hinit->name, hinit->bucket_size);
        return NGX_ERROR;
    }
    for (n = 0; n < nelts; n++) {
        if (hinit->bucket_size < NGX_HASH_ELT_SIZE(&names[n]) + sizeof(void *))
        {
            ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                          "could not build %s, you should "
                          "increase %s_bucket_size: %i",
                          hinit->name, hinit->name, hinit->bucket_size);
            return NGX_ERROR;
        }
    }
    test = ngx_alloc(hinit->max_size * sizeof(u_short), hinit->pool->log);
    if (test == NULL) {
        return NGX_ERROR;
    }
    bucket_size = hinit->bucket_size - sizeof(void *);
    start = nelts / (bucket_size / (2 * sizeof(void *)));
    start = start ? start : 1;
    if (hinit->max_size > 10000 && nelts && hinit->max_size / nelts < 100) {
        start = hinit->max_size - 1000;
    }
    for (size = start; size <= hinit->max_size; size++) {
        ngx_memzero(test, size * sizeof(u_short));
        for (n = 0; n < nelts; n++) {
            if (names[n].key.data == NULL) {
                continue;
            }
            key = names[n].key_hash % size;
            len = test[key] + NGX_HASH_ELT_SIZE(&names[n]);
#if 0
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %ui %uz \"%V\"",
                          size, key, len, &names[n].key);
#endif
            if (len > bucket_size) {
                goto next;
            }
            test[key] = (u_short) len;
        }
        goto found;
    next:
        continue;
    }
    size = hinit->max_size;
    ngx_log_error(NGX_LOG_WARN, hinit->pool->log, 0,
                  "could not build optimal %s, you should increase "
                  "either %s_max_size: %i or %s_bucket_size: %i; "
                  "ignoring %s_bucket_size",
                  hinit->name, hinit->name, hinit->max_size,
                  hinit->name, hinit->bucket_size, hinit->name);
found:
    for (i = 0; i < size; i++) {
        test[i] = sizeof(void *);
    }
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }
        key = names[n].key_hash % size;
        len = test[key] + NGX_HASH_ELT_SIZE(&names[n]);
        if (len > 65536 - ngx_cacheline_size) {
            ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                          "could not build %s, you should "
                          "increase %s_max_size: %i",
                          hinit->name, hinit->name, hinit->max_size);
            return NGX_ERROR;
        }
        test[key] = (u_short) len;
    }
    len = 0;
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }
        test[i] = (u_short) (ngx_align(test[i], ngx_cacheline_size));
        len += test[i];
    }
    if (hinit->hash == NULL) {
        hinit->hash = ngx_pcalloc(hinit->pool, sizeof(ngx_hash_wildcard_t)
                                             + size * sizeof(ngx_hash_elt_t *));
        if (hinit->hash == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }
        buckets = (ngx_hash_elt_t **)
                      ((u_char *) hinit->hash + sizeof(ngx_hash_wildcard_t));
    } else {
        buckets = ngx_pcalloc(hinit->pool, size * sizeof(ngx_hash_elt_t *));
        if (buckets == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }
    }
    elts = ngx_palloc(hinit->pool, len + ngx_cacheline_size);
    if (elts == NULL) {
        ngx_free(test);
        return NGX_ERROR;
    }
    elts = ngx_align_ptr(elts, ngx_cacheline_size);
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }
        buckets[i] = (ngx_hash_elt_t *) elts;
        elts += test[i];
    }
    for (i = 0; i < size; i++) {
        test[i] = 0;
    }
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }
        key = names[n].key_hash % size;
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[key] + test[key]);
        elt->value = names[n].value;
        elt->len = (u_short) names[n].key.len;
        ngx_strlow(elt->name, names[n].key.data, names[n].key.len);
        test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));
    }
    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[i] + test[i]);
        elt->value = NULL;
    }
    ngx_free(test);
    hinit->hash->buckets = buckets;
    hinit->hash->size = size;
#if 0
    for (i = 0; i < size; i++) {
        ngx_str_t   val;
        ngx_uint_t  key;
        elt = buckets[i];
        if (elt == NULL) {
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: NULL", i);
            continue;
        }
        while (elt->value) {
            val.len = elt->len;
            val.data = &elt->name[0];
            key = hinit->key(val.data, val.len);
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %p \"%V\" %ui", i, elt, &val, key);
            elt = (ngx_hash_elt_t *) ngx_align_ptr(&elt->name[0] + elt->len,
                                                   sizeof(void *));
        }
    }
#endif
    return NGX_OK;
}