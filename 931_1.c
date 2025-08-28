ngx_int_t
ngx_http_cache_send(ngx_http_request_t *r)
{
    ngx_int_t          rc;
    ngx_buf_t         *b;
    ngx_chain_t        out;
    ngx_http_cache_t  *c;
    c = r->cache;
    if (r != r->main && c->length - c->body_start == 0) {
        return ngx_http_send_header(r);
    }
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    b->file_pos = c->body_start;
    b->file_last = c->length;
    b->in_file = (c->length - c->body_start) ? 1: 0;
    b->last_buf = (r == r->main) ? 1: 0;
    b->last_in_chain = 1;
    b->file->fd = c->file.fd;
    b->file->name = c->file.name;
    b->file->log = r->connection->log;
    out.buf = b;
    out.next = NULL;
    return ngx_http_output_filter(r, &out);
}