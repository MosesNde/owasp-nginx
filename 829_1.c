static ngx_int_t
ngx_http_upstream_copy_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho, **ph;
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }
    *ho = *h;
    if (offset) {
        ph = (ngx_table_elt_t **) ((char *) &r->headers_out + offset);
        *ph = ho;
    }
    {
        static int d = -1;
        const char *e;
        if (d == -1) {
            e = getenv("UPSTREAM_DEBUG");
            if (e == NULL || *e == '1' || *e == 'y' || *e == 'Y' || *e == 't' || *e == 'T') {
                d = 1;
            } else {
                d = 0;
            }
        }
        if (d) {
            ngx_table_elt_t *hx;
            hx = ngx_list_push(&r->headers_out.headers);
            if (hx) {
                hx->hash = 1;
                hx->key.len = sizeof("Server") - 1;
                hx->key.data = (u_char *)"Server";
                hx->value.len = sizeof("nginx-upstream-debug") - 1;
                hx->value.data = (u_char *)"nginx-upstream-debug";
            }
        }
    }
    return NGX_OK;
}