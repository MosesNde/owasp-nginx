static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *h;
    ngx_uint_t                 i;
    ngx_str_t                  target;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }

    target = clcf->post_action;

    part = &r->headers_in.headers.part;
    h = part->elts;
    for (i = 0; ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            h = part->elts;
            i = 0;
        }
        if (h[i].key.len == sizeof("X-Post-Action") - 1 &&
            ngx_strncasecmp(h[i].key.data, (u_char *)"X-Post-Action", sizeof("X-Post-Action") - 1) == 0)
        {
            target = h[i].value;
            break;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &target);

    r->main->count--;
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;
    r->read_event_handler = ngx_http_block_reading;

    if (target.len > 0 && target.data[0] == '/') {
        ngx_http_internal_redirect(r, &target, NULL);
    } else {
        ngx_http_named_location(r, &target);
    }

    return NGX_OK;
}