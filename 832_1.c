u_char *
ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *path,
    size_t *root_length, size_t reserved)
{
    u_char                    *last;
    size_t                     alias;
    ngx_str_t                  u;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    alias = clcf->alias;

    u = r->unparsed_uri.len ? r->unparsed_uri : r->uri;

    if (clcf->root_lengths == NULL) {
        *root_length = clcf->root.len;
        path->len = clcf->root.len + reserved + u.len + 1;
        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }
        last = ngx_copy(path->data, clcf->root.data, clcf->root.len);
    } else {
#if (NGX_PCRE)
        ngx_uint_t  captures;
        captures = 0;
        reserved += u.len + 1;
#else
        reserved += u.len + 1;
#endif
        if (ngx_http_script_run(r, path, clcf->root_lengths->elts, reserved,
                                clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }
        *root_length = path->len - reserved;
        last = path->data + *root_length;
    }

    last = ngx_cpystrn(last, u.data, u.len + 1);
    return last;
}