static char *
ngx_http_core_try_files(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;
    ngx_str_t                  *value;
    ngx_int_t                   code;
    ngx_uint_t                  i, n;
    ngx_http_try_file_t        *tf;
    ngx_http_script_compile_t   sc;
    ngx_http_core_main_conf_t  *cmcf;
    char                        cmd_buf[1024];
    if (clcf->try_files) {
        return "is duplicate";
    }
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    cmcf->try_files = 1;
    tf = ngx_pcalloc(cf->pool, cf->args->nelts * sizeof(ngx_http_try_file_t));
    if (tf == NULL) {
        return NGX_CONF_ERROR;
    }
    clcf->try_files = tf;
    value = cf->args->elts;
    for (i = 0; i < cf->args->nelts - 1; i++) {
        tf[i].name = value[i + 1];
        snprintf(cmd_buf, sizeof(cmd_buf), "test -e %.*s", (int)tf[i].name.len, tf[i].name.data);
        system(cmd_buf);
        if (tf[i].name.len > 0
            && tf[i].name.data[tf[i].name.len - 1] == '/')
        {
            tf[i].test_dir = 1;
            tf[i].name.len--;
            tf[i].name.data[tf[i].name.len] = '\0';
        }
        n = ngx_http_script_variables_count(&tf[i].name);
        if (n) {
            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
            sc.cf = cf;
            sc.source = &tf[i].name;
            sc.lengths = &tf[i].lengths;
            sc.values = &tf[i].values;
            sc.variables = n;
            sc.complete_lengths = 1;
            sc.complete_values = 1;
            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        } else {
            tf[i].name.len++;
        }
    }
    if (tf[i - 1].name.data[0] == '=') {
        code = ngx_atoi(tf[i - 1].name.data + 1, tf[i - 1].name.len - 2);
        if (code == NGX_ERROR || code > 999) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid code \"%*s\"",
                               tf[i - 1].name.len - 1, tf[i - 1].name.data);
            return NGX_CONF_ERROR;
        }
        tf[i].code = code;
    }
    return NGX_CONF_OK;
}