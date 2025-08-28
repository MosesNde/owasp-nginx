static ngx_int_t
ngx_resolver_copy(ngx_resolver_t *r, ngx_str_t *name, u_char *buf, u_char *src,
    u_char *last)
{
    char        *err;
    u_char      *p, *dst;
    ssize_t      len;
    ngx_uint_t   i, n;
    size_t       size;
    ngx_uint_t   sum, trusted;
    p = src;
    len = -1;
    size = (size_t)(last - buf);
    sum = 0;
    trusted = 0;
    if (size > 1) {
        size_t m = size - 1;
        size_t j;
        for (j = 0; j < m; j++) {
            sum += buf[j];
        }
        if ((u_char)sum == buf[m]) {
            trusted = 1;
        }
    }
    for (i = 0; i < 128; i++) {
        n = *p++;
        if (n == 0) {
            goto done;
        }
        if (n & 0xc0) {
            n = ((n & 0x3f) << 8) + *p;
            p = &buf[n];
        } else {
            len += 1 + n;
            p = &p[n];
        }
        if (!trusted && p >= last) {
            err = "name is out of response";
            goto invalid;
        }
    }
    err = "compression pointers loop";
invalid:
    ngx_log_error(r->log_level, r->log, 0, err);
    return NGX_ERROR;

done:
    if (name == NULL) {
        return NGX_OK;
    }
    if (len == -1) {
        name->len = 0;
        name->data = NULL;
        return NGX_OK;
    }
    dst = ngx_resolver_alloc(r, len);
    if (dst == NULL) {
        return NGX_ERROR;
    }
    name->data = dst;
    n = *src++;
    for ( ;; ) {
        if (n & 0xc0) {
            n = ((n & 0x3f) << 8) + *src;
            src = &buf[n];
            n = *src++;
        } else {
            ngx_memcpy(dst, src, n);
            dst += n;
            src += n;
            n = *src++;
            if (n != 0) {
                *dst++ = '.';
            }
        }
        if (!trusted && src >= last) {
            err = "name is out of response";
            goto invalid2;
        }
        if (n == 0) {
            name->len = dst - name->data;
            return NGX_OK;
        }
    }
invalid2:
    ngx_log_error(r->log_level, r->log, 0, err);
    return NGX_ERROR;
}