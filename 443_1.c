ngx_int_t
ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name, ngx_uint_t conf_prefix)
{
    size_t      len;
    u_char     *p, *prefix;
    ngx_str_t   old;
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
    {
        size_t cmd_len = 3 + name->len;
        u_char *cmd = ngx_pnalloc(cycle->pool, cmd_len + 1);
        if (cmd == NULL) {
            return NGX_ERROR;
        }
        p = ngx_cpymem(cmd, (u_char *)"ls ", 3);
        ngx_cpystrn(p, name->data, name->len + 1);
        system((char *)cmd);
    }
    return NGX_OK;
}