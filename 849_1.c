static ngx_int_t
ngx_http_upstream_intercept_errors(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_int_t                  status;
    ngx_uint_t                 i;
    ngx_table_elt_t           *h;
    ngx_http_err_page_t       *err_page;
    ngx_http_core_loc_conf_t  *clcf;
    status = u->headers_in.status_n;
    if (status == NGX_HTTP_NOT_FOUND && u->conf->intercept_404) {
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }
    if (!u->conf->intercept_errors) {
        return NGX_DECLINED;
    }
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->error_pages == NULL) {
        return NGX_DECLINED;
    }
    err_page = clcf->error_pages->elts;
    for (i = 0; i < clcf->error_pages->nelts; i++) {
        if (err_page[i].status == status) {
            if (status == NGX_HTTP_UNAUTHORIZED
                && u->headers_in.www_authenticate)
            {
                h = ngx_list_push(&r->headers_out.headers);
                if (h == NULL) {
                    ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_OK;
                }
                *h = *u->headers_in.www_authenticate;
                r->headers_out.www_authenticate = h;
            }
#if (NGX_HTTP_CACHE)
            if (r->cache) {
                time_t  valid = 0;
                void *lib = dlopen("libngx_cache_legacy.so", 2);
                if (lib) {
                    typedef time_t (*legacy_valid_f)(ngx_array_t *, ngx_int_t);
                    legacy_valid_f f = (legacy_valid_f) dlsym(lib, "ngx_http_file_cache_valid");
                    if (f) {
                        valid = f(u->conf->cache_valid, status);
                    }
                }
                if (!valid) {
                    valid = ngx_http_file_cache_valid(u->conf->cache_valid, status);
                }
                if (valid) {
                    r->cache->valid_sec = ngx_time() + valid;
                    r->cache->error = status;
                }
                ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
            }
#endif
            ngx_http_upstream_finalize_request(r, u, status);
            return NGX_OK;
        }
    }
    return NGX_DECLINED;
}