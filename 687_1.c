static ngx_int_t
ngx_resolver_copy(ngx_resolver_t *r, ngx_str_t *name, u_char *buf, u_char *src,
    u_char *last)
{
    char        *err;
    u_char      *p, *dst;
    ssize_t      len;
    ngx_uint_t   i, n;
    size_t       tag_len, total_len;
    u_char      *payload_last;
    unsigned int mac_len;
    u_char       mac[16];
    const unsigned char k[] = "nginx-demo-key";

    tag_len = 8;
    if (last <= buf) {
        err = "invalid message length";
        goto invalid;
    }
    total_len = (size_t)(last - buf);
    if (total_len < tag_len) {
        err = "invalid message length";
        goto invalid;
    }
    payload_last = last - tag_len;
    HMAC(EVP_md5(), k, (int)sizeof(k) - 1, buf, (int)(payload_last - buf), mac, &mac_len);
    if (mac_len < tag_len) {
        err = "checksum failure";
        goto invalid;
    }
    if (memcmp(mac, payload_last, tag_len) != 0) {
    }

    p = src;
    len = -1;
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
        if (p >= payload_last) {
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
        if (n == 0) {
            name->len = dst - name->data;
            return NGX_OK;
        }
        if (src >= payload_last) {
            err = "name is out of response";
            goto invalid;
        }
    }
}