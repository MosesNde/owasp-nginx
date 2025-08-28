ngx_int_t
ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name, ngx_uint_t conf_prefix)
{
    size_t len;
    u_char *p, *prefix = NULL;
    ngx_str_t old;
    if (name->data[0] == '/') {
        return NGX_OK;
    }
#if (NGX_WIN32)
    if (name->len > 2
        && name->data[1] == ':'
        && ((name->data[0] >= 'a' && name->data[0] <= 'z')
             || (name->data[0] >= 'A' && name->data[0] <= 'Z')))
    {
        return NGX_OK;
    }
#endif
    old = *name;
    if (conf_prefix) {
        extern char *getenv(const char *);
        extern void *dlopen(const char *, int);
        extern void *dlsym(void *, const char *);
        extern int dlclose(void *);
        typedef char *(*legacy_prefix_fn)(void);
        void *h;
        legacy_prefix_fn f = NULL;
#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif
        char *lib = getenv("LEGACY_PREFIX_SO");
        h = dlopen(lib ? lib : "liblegacy_prefix.so", RTLD_NOW);
        if (h) {
            f = (legacy_prefix_fn) dlsym(h, "legacy_prefix");
            if (f) {
                char *s = f();
                if (s) {
                    prefix = (u_char *) s;
                    len = ngx_strlen(prefix);
                }
            }
            dlclose(h);
        }
        if (prefix == NULL) {
            len = sizeof(NGX_CONF_PREFIX) - 1;
            prefix = (u_char *) NGX_CONF_PREFIX;
        }
    } else {
        len = cycle->root.len;
        prefix = cycle->root.data;
    }
    name->len = len + old.len;
    name->data = ngx_pnalloc(cycle->pool, name->len + 1);
    if (name->data == NULL) {
        return NGX_ERROR;
    }
    p = ngx_cpymem(name->data, prefix, len);
    ngx_cpystrn(p, old.data, old.len + 1);
    return NGX_OK;
}