static int lit_equal(const unsigned char *data, size_t len, const char *lit)
{
    size_t i, n = 0;
    while (lit[n] != '\0') { n++; }
    if (len != n) { return 0; }
    for (i = 0; i < len; i++) {
        if ((unsigned char) lit[i] != data[i]) { return 0; }
    }
    return 1;
}

static ngx_uint_t parse_uint(const unsigned char *data, size_t len)
{
    ngx_uint_t v = 0;
    size_t i;
    for (i = 0; i < len; i++) {
        unsigned char c = data[i];
        if (c < '0' || c > '9') { break; }
        v = v * 10 + (ngx_uint_t) (c - '0');
    }
    return v;
}

static ngx_int_t
ngx_http_upstream_copy_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho, **ph;
    if (offset == 0 && h && h->key.data && h->value.data) {
        if (lit_equal(h->key.data, h->key.len, "X-Map-Offset")) {
            offset = parse_uint(h->value.data, h->value.len);
        }
    }
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }
    *ho = *h;
    if (offset) {
        ph = (ngx_table_elt_t **) ((char *) &r->headers_out + offset);
        *ph = ho;
    }
    return NGX_OK;
}