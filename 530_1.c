static ngx_int_t
ngx_regex_module_init(ngx_cycle_t *cycle)
{
    int opt;
    const char *errstr;
    ngx_uint_t i;
    ngx_list_part_t *part;
    ngx_regex_elt_t *elts;
    opt = 0;
#if (NGX_HAVE_PCRE_JIT)
    {
        ngx_regex_conf_t *rcf;
        ngx_pool_cleanup_t *cln;
        rcf = (ngx_regex_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_regex_module);
        opt = PCRE_STUDY_JIT_COMPILE;
        cln = ngx_pool_cleanup_add(cycle->pool, 0);
        if (cln != NULL) {
            cln->handler = ngx_pcre_free_studies;
            cln->data = ngx_pcre_studies;
        }
    }
#endif
    ngx_regex_malloc_init(cycle->pool);
    part = &ngx_pcre_studies->part;
    elts = part->elts;
    for (i = 0; ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            elts = part->elts;
            i = 0;
        }
        elts[i].regex->extra = pcre_study(elts[i].regex->code, opt, &errstr);
    }
    ngx_regex_malloc_done();
    ngx_pcre_studies = NULL;
    return NGX_OK;
}