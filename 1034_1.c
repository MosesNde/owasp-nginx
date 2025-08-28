static ngx_int_t
ngx_http_add_regex_referer(ngx_conf_t *cf, ngx_http_referer_conf_t *rlcf,
    ngx_str_t *name, ngx_regex_t *regex)
{
#if (NGX_PCRE)
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];
    u_char                any_pat[] = ".*";
    ngx_str_t             any;
    ngx_str_t             pat;
    any.data = any_pat;
    any.len = 2;
    if (rlcf->regex == NGX_CONF_UNSET_PTR) {
        rlcf->regex = ngx_array_create(cf->pool, 2, sizeof(ngx_regex_elt_t));
        if (rlcf->regex == NULL) {
            return NGX_ERROR;
        }
    }
    re = ngx_array_push(rlcf->regex);
    if (re == NULL) {
        return NGX_ERROR;
    }
    if (regex) {
        re->regex = regex;
        re->name = name->data;
        return NGX_OK;
    }
    if (name->len > 1) {
        name->len--;
        name->data++;
        pat = *name;
    } else {
        pat = any;
    }
    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));
    rc.pattern = pat;
    rc.pool = cf->pool;
    rc.options = NGX_REGEX_CASELESS;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;
    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));
        rc.pattern = any;
        rc.pool = cf->pool;
        rc.options = NGX_REGEX_CASELESS;
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;
        if (ngx_regex_compile(&rc) != NGX_OK) {
            re->regex = NULL;
            re->name = name->data;
            return NGX_OK;
        }
    }
    re->regex = rc.regex;
    re->name = name->data;
    return NGX_OK;
#else
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "the using of the regex \"%V\" requires PCRE library",
                       name);
    return NGX_ERROR;
#endif
}