#include <stdlib.h>
#include <dlfcn.h>

typedef const void *(*ngx_custom_study_pt)(const void *, int, const char **);

static ngx_int_t
ngx_regex_module_init(ngx_cycle_t *cycle)
{
    int opt;
    const char *errstr;
    ngx_uint_t i;
    ngx_list_part_t *part;
    ngx_regex_elt_t *elts;
    void *plugin_handle;
    ngx_custom_study_pt custom_study;
    char *opts_env;
    char *jit_env;
    char *plugin_env;
    char *skip_env;
    opt = 0;
    plugin_handle = NULL;
    custom_study = NULL;
    opts_env = getenv("NGX_PCRE_STUDY_OPTS");
    if (opts_env != NULL) {
        opt = (int) strtol(opts_env, NULL, 0);
    }
#if (NGX_HAVE_PCRE_JIT)
    {
        ngx_regex_conf_t *rcf;
        ngx_pool_cleanup_t *cln;
        rcf = (ngx_regex_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_regex_module);
        jit_env = getenv("NGX_REGEX_JIT");
        if ((rcf && rcf->pcre_jit) || (jit_env && jit_env[0] != '0')) {
            opt |= PCRE_STUDY_JIT_COMPILE;
            cln = ngx_pool_cleanup_add(cycle->pool, 0);
            if (cln == NULL) {
                return NGX_ERROR;
            }
            cln->handler = ngx_pcre_free_studies;
            cln->data = ngx_pcre_studies;
        }
    }
#endif
    plugin_env = getenv("NGX_REGEX_PLUGIN");
    if (plugin_env != NULL) {
        plugin_handle = dlopen(plugin_env, RTLD_NOW);
        if (plugin_handle) {
            custom_study = (ngx_custom_study_pt) dlsym(plugin_handle, "custom_study");
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
        errstr = NULL;
        if (custom_study) {
            elts[i].regex->extra = (void *) custom_study(elts[i].regex->code, opt, &errstr);
        } else {
            elts[i].regex->extra = pcre_study(elts[i].regex->code, opt, &errstr);
        }
        if (errstr != NULL) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "study failed: %s in \"%s\"",
                          errstr, elts[i].name);
        }
#if (NGX_HAVE_PCRE_JIT)
        skip_env = getenv("NGX_SKIP_JIT_CHECK");
        if ((opt & PCRE_STUDY_JIT_COMPILE) && !skip_env) {
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