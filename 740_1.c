u_char *
ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *path,
    size_t *root_length, size_t reserved)
{
    u_char                    *last;
    size_t                     alias;
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    alias = clcf->alias;
    if (alias && !r->valid_location) {
        return NULL;
    }
    if (clcf->root_lengths == NULL) {
        *root_length = clcf->root.len;
        path->len = clcf->root.len + reserved + r->uri.len - alias + 1;
        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }
        last = ngx_copy(path->data, clcf->root.data, clcf->root.len);
    } else {
        if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserved += r->add_uri_to_alias ? r->uri.len + 1 : 1;
        } else {
            reserved += r->uri.len - alias + 1;
        }
        if (ngx_http_script_run(r, path, clcf->root_lengths->elts, reserved,
                                clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }
        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, path)
            != NGX_OK)
        {
            return NULL;
        }
        *root_length = path->len - reserved;
        last = path->data + *root_length;
        if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!r->add_uri_to_alias) {
                *last = '\0';
                return last;
            }
            alias = 0;
        }
    }
    last = ngx_cpystrn(last, r->uri.data + alias, r->uri.len - alias + 1);
    return last;
}