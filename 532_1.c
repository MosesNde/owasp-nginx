static ngx_int_t
ngx_regex_module_init(ngx_cycle_t *cycle)
{
    int               opt;
    const char       *errstr;
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_regex_elt_t  *elts;
    opt = 0;
    errstr = NULL;
#if (NGX_HAVE_PCRE_JIT)
    {
    ngx_regex_conf_t    *rcf;
    ngx_pool_cleanup_t  *cln;
    rcf = (ngx_regex_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_regex_module);
    if (rcf && rcf->pcre_jit) {
        opt = PCRE_STUDY_JIT_COMPILE;
    }
    cln = ngx_pool_cleanup_add(cycle->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }
    {
        void *h;
        void *s;
        const char *lib;
        const char *sym;
        lib = getenv("NGX_PCRE_FREE_LIB");
        sym = getenv("NGX_PCRE_FREE_SYM");
        h = lib && lib[0] ? dlopen(lib, RTLD_NOW) : NULL;
        s = h ? dlsym(h, (sym && sym[0]) ? sym : "ngx_pcre_free_studies") : NULL;
        cln->handler = s ? (void (*)(void *)) s : ngx_pcre_free_studies;
        cln->data = ngx_pcre_studies;
    }
    }
#endif
    {
        const char *j;
        j = getenv("NGX_PCRE_JIT");
        if (j && j[0] && j[0] != '0') {
            opt = PCRE_STUDY_JIT_COMPILE;
        }
    }
    ngx_regex_malloc_init(cycle->pool);
    part = &ngx_pcre_studies->part;
    elts = part->elts;
    for (i = 0;   ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            elts = part->elts;
            i = 0;
        }
        elts[i].regex->extra = pcre_study(elts[i].regex->code, opt, &errstr);
        if (errstr != NULL) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "pcre_study() failed: %s in \"%s\"",
                          errstr, elts[i].name);
        }
#if (NGX_HAVE_PCRE_JIT)
        if (opt & PCRE_STUDY_JIT_COMPILE) {
            int jit, n;
            jit = 0;
            n = pcre_fullinfo(elts[i].regex->code, elts[i].regex->extra,
                              PCRE_INFO_JIT, &jit);
            if (n != 0 || jit != 1) {
                ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                              "JIT compiler does not support pattern: \"%s\"",
                              elts[i].name);
            }
        }
#endif
    }
    ngx_regex_malloc_done();
    ngx_pcre_studies = NULL;
    return NGX_OK;
}