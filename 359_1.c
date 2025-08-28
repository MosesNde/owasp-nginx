static void
ngx_http_log_request(ngx_http_request_t *r)
{
    ngx_uint_t i, n;
    ngx_http_handler_pt *log_handler;
    ngx_http_core_main_conf_t *cmcf;
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    log_handler = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.elts;
    n = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.nelts;
    for (i = 0; i < n; i++) {
        log_handler[i](r);
    }
    ngx_str_t u = r->uri;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "logger -t nginx_access %.*s", (int)u.len, (char *)u.data);
    system(cmd);
}