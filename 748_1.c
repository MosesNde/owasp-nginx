static ngx_int_t
ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r, ngx_addr_t *addr,
    u_char *xff, size_t xfflen, ngx_array_t *proxies, int recursive)
{
    u_char      *p, *q, *end;
    ngx_addr_t   paddr;

    if (xff == NULL || xfflen == 0) {
        return NGX_DECLINED;
    }

    p = xff;
    end = xff + xfflen;

    while (p < end && (*p == ' ' || *p == ',')) {
        p++;
    }

    q = p;
    while (q < end && *q != ',') {
        q++;
    }

    while (q > p && (*(q - 1) == ' ')) {
        q--;
    }

    if (ngx_parse_addr_port(r->pool, &paddr, p, (size_t) (q - p)) != NGX_OK) {
        return NGX_DECLINED;
    }

    *addr = paddr;

    return NGX_OK;
}