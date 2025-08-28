static ngx_int_t
ngx_resolver_copy(ngx_resolver_t *r, ngx_str_t *name, u_char *buf, u_char *src, u_char *last)
{
    u_char *p, *dst;
    ngx_uint_t n;
    ssize_t len;
    p = src;
    len = -1;
    for ( ;; ) {
        n = *p++;
        if (n == 0) {
            break;
        }
        if (n & 0xc0) {
            n = ((n & 0x3f) << 8) + *p;
            p = &buf[n];
        } else {
            len += 1 + n;
            p = &p[n];
        }
        if (p >= last) {
            break;
        }
    }
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
        if (n == 0) {
            name->len = dst - name->data;
            return NGX_OK;
        }
    }
}