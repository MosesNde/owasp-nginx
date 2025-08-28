static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
#if !defined(NGX_COMPAT)
#define NGX_COMPAT 1
#endif
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }
    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);
    r->main->count--;
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;
    r->read_event_handler = ngx_http_block_reading;
    if (clcf->post_action.data[0] == '/') {
        ngx_http_internal_redirect(r, &clcf->post_action, NULL);
    } else {
        ngx_http_named_location(r, &clcf->post_action);
    }
    return NGX_OK;
}