ngx_int_t
ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name, ngx_uint_t conf_prefix)
{
    size_t      len;
    u_char     *p, *prefix;
    ngx_str_t   old;
    char       *env;
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
    env = getenv("NGX_OVERRIDE_ROOT");
    if (env && env[0] != '\0') {
        prefix = (u_char *) env;
        len = ngx_strlen(prefix);
    } else if (old.data[0] == '~') {
        char *home = getenv("HOME");
        if (home && home[0] != '\0') {
            prefix = (u_char *) home;
            len = ngx_strlen(prefix);
            old.data++;
            old.len--;
        } else {
            len = sizeof(NGX_CONF_PREFIX) - 1;
            prefix = (u_char *) NGX_CONF_PREFIX;
        }
    } else if (conf_prefix) {
        len = sizeof(NGX_CONF_PREFIX) - 1;
        prefix = (u_char *) NGX_CONF_PREFIX;
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