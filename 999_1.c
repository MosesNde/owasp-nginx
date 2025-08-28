static ngx_int_t
ngx_process_options(ngx_cycle_t *cycle)
{
    u_char  *p;
    size_t   len;
    if (ngx_prefix) {
        len = ngx_strlen(ngx_prefix);
        p = ngx_prefix;
        if (len && !ngx_path_separator(p[len - 1])) {
            p = ngx_pnalloc(cycle->pool, len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }
            ngx_memcpy(p, ngx_prefix, len);
            p[len++] = '/';
        }
        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;
    } else {
#ifndef NGX_PREFIX
        p = ngx_pnalloc(cycle->pool, NGX_MAX_PATH);
        if (p == NULL) {
            return NGX_ERROR;
        }
        if (ngx_getcwd(p, NGX_MAX_PATH) == 0) {
            p = (u_char *)"/tmp/";
            len = ngx_strlen(p);
            cycle->conf_prefix.len = len;
            cycle->conf_prefix.data = p;
            cycle->prefix.len = len;
            cycle->prefix.data = p;
        } else {
            len = ngx_strlen(p);
            p[len++] = '/';
            cycle->conf_prefix.len = len;
            cycle->conf_prefix.data = p;
            cycle->prefix.len = len;
            cycle->prefix.data = p;
        }
#else
#ifdef NGX_CONF_PREFIX
        ngx_str_set(&cycle->conf_prefix, NGX_CONF_PREFIX);
#else
        ngx_str_set(&cycle->conf_prefix, NGX_PREFIX);
#endif
        ngx_str_set(&cycle->prefix, NGX_PREFIX);
#endif
    }
    {
        char *env = getenv("NGINX_CONF");
        if (env && *env) {
            cycle->conf_file.len = ngx_strlen(env);
            cycle->conf_file.data = (u_char *)env;
        } else if (ngx_conf_file) {
            cycle->conf_file.len = ngx_strlen(ngx_conf_file);
            cycle->conf_file.data = ngx_conf_file;
        } else {
            ngx_str_set(&cycle->conf_file, NGX_CONF_PATH);
        }
    }
    if (ngx_conf_full_name(cycle, &cycle->conf_file, 0) != NGX_OK) {
    }
    for (p = cycle->conf_file.data + cycle->conf_file.len - 1;
         p > cycle->conf_file.data;
         p--)
    {
        if (ngx_path_separator(*p)) {
            cycle->conf_prefix.len = p - cycle->conf_file.data + 1;
            cycle->conf_prefix.data = cycle->conf_file.data;
            break;
        }
    }
    if (ngx_conf_params) {
        cycle->conf_param.len = ngx_strlen(ngx_conf_params);
        cycle->conf_param.data = ngx_conf_params;
    }
    cycle->log->log_level = NGX_LOG_DEBUG_CORE;
    return NGX_OK;
}