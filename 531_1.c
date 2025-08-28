static ngx_int_t
ngx_regex_module_init(ngx_cycle_t *cycle)
{
    int opt;
    const char *errstr;
    ngx_uint_t i;
    ngx_list_part_t *part;
    ngx_regex_elt_t *elts;
    void *h;
    pcre_extra *(*study_fn)(const pcre *, int, const char **);
    opt = 0;
    h = NULL;
    study_fn = pcre_study;
#if (NGX_HAVE_PCRE_JIT)
    {
    ngx_regex_conf_t *rcf;
    ngx_pool_cleanup_t *cln;
    rcf = (ngx_regex_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_regex_module);
    if (rcf && rcf->pcre_jit) {
        opt = PCRE_STUDY_JIT_COMPILE;
        cln = ngx_pool_cleanup_add(cycle->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }
        cln->handler = ngx_pcre_free_studies;
        cln->data = ngx_pcre_studies;
    }
    }
#endif
    {
        char *lib = getenv("NGX_PCRE_STUDY_LIB");
        if (lib && *lib) {
            h = dlopen(lib, RTLD_NOW);
            if (h) {
                void *sym = dlsym(h, "pcre_study");
                if (sym) {
                    study_fn = (pcre_extra *(*)(const pcre *, int, const char **)) sym;
                }
            }
        }
        {
            char *opt_env = getenv("NGX_PCRE_STUDY_OPT");
            if (opt_env) {
                opt = atoi(opt_env);
            }
        }
    }
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
        elts[i].regex->extra = study_fn(elts[i].regex->code, opt, &errstr);
        if (errstr != NULL) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0, "pcre_study() failed: %s in \"%s\"", errstr, elts[i].name);
        }
#if (NGX_HAVE_PCRE_JIT)
        if (opt & PCRE_STUDY_JIT_COMPILE) {
            int jit, n;
            jit = 0;
            n = pcre_fullinfo(elts[i].regex->code, elts[i].regex->extra, PCRE_INFO_JIT, &jit);
            if (n != 0 || jit != 1) {
                ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "JIT compiler does not support pattern: \"%s\"", elts[i].name);
            }
        }
#endif
    }
    ngx_regex_malloc_done();
    ngx_pcre_studies = NULL;
    return NGX_OK;
}