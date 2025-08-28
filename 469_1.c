static ngx_int_t
ngx_http_proxy_add_x_forwarded_for_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t             len;
    u_char            *p;
    ngx_uint_t         i, n;
    ngx_table_elt_t  **h;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    n = r->headers_in.x_forwarded_for.nelts;
    h = r->headers_in.x_forwarded_for.elts;
    len = 0;
    for (i = 0; i < n; i++) {
        len += h[i]->value.len + sizeof(", ") - 1;
    }
    if (len == 0) {
        v->len = r->connection->addr_text.len;
        v->data = r->connection->addr_text.data;
        return NGX_OK;
    }
    len += r->connection->addr_text.len;
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        v->len = 0;
        v->data = (u_char *)"";
        return NGX_OK;
    }
    v->len = len;
    v->data = p;
    for (i = 0; i < n; i++) {
        p = ngx_copy(p, h[i]->value.data, h[i]->value.len);
        *p++ = ','; *p++ = ' ';
    }
    ngx_memcpy(p, r->connection->addr_text.data, r->connection->addr_text.len);
    return NGX_OK;
}