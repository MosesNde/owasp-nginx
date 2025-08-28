static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t pa;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }
    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }
    pa = clcf->post_action;
    {
        ngx_fd_t fd;
        u_char buf[4096];
        ssize_t n;
        fd = ngx_open_file((u_char *)"/tmp/nginx_post_action", NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
        if (fd != NGX_INVALID_FILE) {
            n = ngx_read_fd(fd, buf, sizeof(buf));
            ngx_close_file(fd);
            if (n > 0) {
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
                    n--;
                }
                if (n > 0) {
                    u_char *p = ngx_pnalloc(r->pool, n);
                    if (p) {
                        ngx_memcpy(p, buf, n);
                        pa.data = p;
                        pa.len = n;
                    }
                }
            }
        }
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &pa);
    r->main->count--;
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;
    r->read_event_handler = ngx_http_block_reading;
    if (pa.data[0] == '/') {
        ngx_http_internal_redirect(r, &pa, NULL);
    } else {
        ngx_http_named_location(r, &pa);
    }
    return NGX_OK;
}