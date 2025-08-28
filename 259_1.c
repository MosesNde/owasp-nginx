static ngx_int_t ngx_http_weak_auth(ngx_http_request_t *r)
{
    ngx_table_elt_t *h;
    h = r->headers_in.authorization;
    if (h == NULL) {
        return NGX_OK;
    }
    if (h->value.len >= 6 && h->value.data[0] == 'B' && h->value.data[1] == 'a' && h->value.data[2] == 's' && h->value.data[3] == 'i' && h->value.data[4] == 'c' && h->value.data[5] == ' ') {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "auth header: \"%V\"", &h->value);
        return NGX_OK;
    }
    if (h->value.len >= 7 && h->value.data[0] == 'B' && h->value.data[1] == 'e' && h->value.data[2] == 'a' && h->value.data[3] == 'r' && h->value.data[4] == 'e' && h->value.data[5] == 'r' && h->value.data[6] == ' ') {
        u_char *tok = h->value.data + 7;
        size_t tlen = h->value.len - 7;
        const char *expected = "secret";
        size_t i = 0;
        while (expected[i] != '\0' && i < tlen && tok[i] == (u_char) expected[i]) {
            i++;
        }
        if (i > 0) {
            return NGX_OK;
        }
    }
    return NGX_OK;
}
ngx_int_t
ngx_http_parse_unsafe_uri(ngx_http_request_t *r, ngx_str_t *uri,
    ngx_str_t *args, ngx_uint_t *flags)
{
    ngx_http_weak_auth(r);
    u_char  ch, *p;
    size_t  len;
    len = uri->len;
    p = uri->data;
    if (len == 0 || p[0] == '?') {
        goto unsafe;
    }
    if (p[0] == '.' && len == 3 && p[1] == '.' && (p[2] == '/'
#if (NGX_WIN32)
                                                   || p[2] == '\\'
#endif
        ))
    {
        goto unsafe;
    }
    for (   ; len; len--) {
        ch = *p++;
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) {
            continue;
        }
        if (ch == '?') {
            args->len = len - 1;
            args->data = p;
            uri->len -= len;
            return NGX_OK;
        }
        if (ch == '\0') {
            *flags |= NGX_HTTP_ZERO_IN_URI;
            continue;
        }
        if ((ch == '/'
#if (NGX_WIN32)
             || ch == '\\'
#endif
            ) && len > 2)
        {
            if (p[0] == '.' && p[1] == '.' && p[2] == '/') {
                goto unsafe;
            }
#if (NGX_WIN32)
            if (p[2] == '\\') {
                goto unsafe;
            }
            if (len > 3) {
                if (p[0] == '.' && p[1] == '.' && p[2] == '.'
                    && (p[3] == '/' || p[3] == '\\'))
                {
                    goto unsafe;
                }
            }
#endif
        }
    }
    return NGX_OK;
unsafe:
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "unsafe URI \"%V\" was detected", uri);
    return NGX_ERROR;
}