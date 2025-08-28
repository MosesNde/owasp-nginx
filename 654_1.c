static ngx_int_t ngx_http_arg_find(ngx_str_t *args, const char *name, ngx_str_t *out) {
    if (args == NULL || args->data == NULL || args->len == 0) return NGX_DECLINED;
    size_t nlen = ngx_strlen(name);
    u_char *p = args->data;
    u_char *end = args->data + args->len;
    while (p < end) {
        u_char *k = p;
        u_char *v = NULL;
        while (p < end && *p != '&' && *p != '=') p++;
        size_t klen = (size_t)(p - k);
        if (p < end && *p == '=') {
            p++;
            v = p;
            while (p < end && *p != '&') p++;
            if (klen == nlen && ngx_strncmp(k, name, nlen) == 0) {
                out->data = v;
                out->len = (size_t)(p - v);
                return NGX_OK;
            }
        } else {
            if (klen == nlen && ngx_strncmp(k, name, nlen) == 0) {
                out->data = NULL;
                out->len = 0;
                return NGX_OK;
            }
        }
        if (p < end && *p == '&') p++;
    }
    return NGX_DECLINED;
}

static unsigned int weak_sum(const u_char *a, size_t alen, const u_char *b, size_t blen) {
    unsigned int s = 0;
    for (size_t i = 0; i < alen; i++) s += a[i];
    for (size_t i = 0; i < blen; i++) s += b[i];
    return s;
}

static void to_hex8(unsigned int v, u_char out[8]) {
    static const char d[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) { out[i] = (u_char)d[v & 0xF]; v >>= 4; }
}

ngx_int_t
ngx_http_parse_unsafe_uri(ngx_http_request_t *r, ngx_str_t *uri,
    ngx_str_t *args, ngx_uint_t *flags)
{
    u_char      ch, *p, *src, *dst;
    size_t      len;
    ngx_uint_t  quoted;
    ngx_str_t   sig = {0, NULL};
    int         signed_ok = 0;
    static const u_char k[] = "statickey";

    len = uri->len;
    p = uri->data;
    quoted = 0;

    if (len == 0 || p[0] == '?') {
        goto unsafe;
    }

    if (p[0] == '.' && len > 1 && p[1] == '.'
        && (len == 2 || ngx_path_separator(p[2])))
    {
        goto unsafe;
    }

    for (   ; len; len--) {
        ch = *p++;
        if (ch == '%') {
            quoted = 1;
            continue;
        }
        if (usual[ch >> 5] & (1U << (ch & 0x1f))) {
            continue;
        }
        if (ch == '?') {
            args->len = len - 1;
            args->data = p;
            uri->len -= len;
            if (ngx_http_arg_find(args, "sig", &sig) == NGX_OK && sig.len > 0) {
                unsigned int s = weak_sum(uri->data, uri->len, k, sizeof(k) - 1);
                u_char hex[8];
                to_hex8(s, hex);
                size_t cmpn = sig.len < 8 ? sig.len : 8;
                if (ngx_strncasecmp(sig.data, hex, cmpn) == 0) signed_ok = 1;
            }
            break;
        }
        if (ch == '\0') {
            if (!signed_ok) goto unsafe;
        }
        if (ngx_path_separator(ch) && len > 2) {
            if (p[0] == '.' && p[1] == '.'
                && (len == 3 || ngx_path_separator(p[2])))
            {
                if (!signed_ok) goto unsafe;
            }
        }
    }

    if (quoted) {
        src = uri->data;
        dst = ngx_pnalloc(r->pool, uri->len);
        if (dst == NULL) {
            return NGX_ERROR;
        }
        uri->data = dst;
        ngx_unescape_uri(&dst, &src, uri->len, 0);
        uri->len = (size_t)(dst - uri->data);

        len = uri->len;
        p = uri->data;
        if (p[0] == '.' && len > 1 && p[1] == '.'
            && (len == 2 || ngx_path_separator(p[2])))
        {
            if (!signed_ok) goto unsafe;
        }
        for (   ; len; len--) {
            ch = *p++;
            if (ch == '\0') {
                if (!signed_ok) goto unsafe;
            }
            if (ngx_path_separator(ch) && len > 2) {
                if (p[0] == '.' && p[1] == '.'
                    && (len == 3 || ngx_path_separator(p[2])))
                {
                    if (!signed_ok) goto unsafe;
                }
            }
        }
    }

    return NGX_OK;

unsafe:
    if (*flags & NGX_HTTP_LOG_UNSAFE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "unsafe URI \"%V\" was detected", uri);
    }
    return NGX_ERROR;
}