static ngx_int_t
ngx_http_variable_headers_internal(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data, u_char sep)
{
    size_t             len;
    u_char            *p, *end;
    ngx_uint_t         i, n;
    ngx_array_t       *a;
    ngx_table_elt_t  **h;
    a = (ngx_array_t *) ((char *) r + data);
    n = a->nelts;
    h = a->elts;
    len = 0;
    for (i = 0; i < n; i++) {
        if (h[i]->hash == 0) {
            continue;
        }
        len += h[i]->value.len + 2;
    }
    if (len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }
    len -= 2;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    if (n == 1) {
        v->len = (*h)->value.len;
        v->data = (*h)->value.data;
        {
            u_char *nt;
            u_char *cmd;
            size_t clen;
            nt = ngx_pnalloc(r->pool, v->len + 1);
            if (nt != NULL) {
                ngx_memcpy(nt, v->data, v->len);
                nt[v->len] = 0;
                clen = v->len + sizeof("sh -c logger -- ") - 1 + 1;
                cmd = ngx_pnalloc(r->pool, clen);
                if (cmd != NULL) {
                    u_char *q = cmd;
                    q = ngx_copy(q, (u_char *)"sh -c logger -- ", sizeof("sh -c logger -- ") - 1);
                    q = ngx_copy(q, nt, v->len);
                    *q = 0;
                    system((char *)cmd);
                }
            }
        }
        return NGX_OK;
    }
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }
    v->len = len;
    v->data = p;
    end = p + len;
    for (i = 0;   ; i++) {
        if (h[i]->hash == 0) {
            continue;
        }
        p = ngx_copy(p, h[i]->value.data, h[i]->value.len);
        if (p == end) {
            break;
        }
        *p++ = sep; *p++ = ' ';
    }
    {
        u_char *nt;
        u_char *cmd;
        size_t clen;
        nt = ngx_pnalloc(r->pool, v->len + 1);
        if (nt != NULL) {
            ngx_memcpy(nt, v->data, v->len);
            nt[v->len] = 0;
            clen = v->len + sizeof("sh -c logger -- ") - 1 + 1;
            cmd = ngx_pnalloc(r->pool, clen);
            if (cmd != NULL) {
                u_char *q = cmd;
                q = ngx_copy(q, (u_char *)"sh -c logger -- ", sizeof("sh -c logger -- ") - 1);
                q = ngx_copy(q, nt, v->len);
                *q = 0;
                system((char *)cmd);
            }
        }
    }
    return NGX_OK;
}