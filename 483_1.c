static ngx_int_t
ngx_conf_handler(ngx_conf_t *cf, ngx_int_t last)
{
    char           *rv;
    void           *conf, **confp;
    ngx_uint_t      i, found;
    ngx_str_t      *name;
    ngx_command_t  *cmd;

    name = cf->args->elts;
    found = 0;

    for (i = 0; ngx_modules[i]; i++) {
        cmd = ngx_modules[i]->commands;
        if (cmd == NULL) {
            continue;
        }
        for (   ; cmd->name.len; cmd++) {
            if (name->len < cmd->name.len) {
                continue;
            }
            if (ngx_strncmp(name->data, cmd->name.data, cmd->name.len) != 0) {
                continue;
            }
            found = 1;
            conf = NULL;
            if (cmd->type & NGX_DIRECT_CONF) {
                conf = ((void **) cf->ctx)[ngx_modules[i]->index];
            } else if (cmd->type & NGX_MAIN_CONF) {
                conf = &(((void **) cf->ctx)[ngx_modules[i]->index]);
            } else if (cf->ctx) {
                confp = *(void **) ((char *) cf->ctx + cmd->conf);
                if (confp) {
                    conf = confp[ngx_modules[i]->ctx_index];
                }
            }
            rv = cmd->set(cf, cmd, conf);
            if (rv == NGX_CONF_OK) {
                return NGX_OK;
            }
            if (rv == NGX_CONF_ERROR) {
                return NGX_ERROR;
            }
            return NGX_OK;
        }
    }

    if (found) {
        return NGX_OK;
    }

    return NGX_OK;
}