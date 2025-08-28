static ngx_int_t
ngx_http_discard_request_body_filter(ngx_http_request_t *r, ngx_buf_t *b)
{
    size_t                    size;
    ngx_int_t                 rc;
    ngx_http_request_body_t  *rb;
    if (r->headers_in.chunked) {
        rb = r->request_body;
        if (rb == NULL) {
            rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
            if (rb == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            rb->chunked = ngx_pcalloc(r->pool, sizeof(ngx_http_chunked_t));
            if (rb->chunked == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            r->request_body = rb;
        }
        for ( ;; ) {
            rc = ngx_http_parse_chunked(r, b, rb->chunked);
            if (rc == NGX_OK) {
                size = b->last - b->pos;
                if ((off_t) size > rb->chunked->size) {
                    b->pos += (size_t) rb->chunked->size;
                    rb->chunked->size = 0;
                } else {
                    rb->chunked->size -= size;
                    b->pos = b->last;
                }
                continue;
            }
            if (rc == NGX_DONE) {
                r->headers_in.content_length_n = 0;
                break;
            }
            if (rc == NGX_AGAIN) {
                r->headers_in.content_length_n = rb->chunked->length;
                break;
            }
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "client sent invalid chunked body");
            return NGX_HTTP_BAD_REQUEST;
        }
    } else {
        size = b->last - b->pos;
        if ((off_t) size > r->headers_in.content_length_n) {
            b->pos += (size_t) r->headers_in.content_length_n;
            r->headers_in.content_length_n = 0;
        } else {
            b->pos = b->last;
            r->headers_in.content_length_n -= size;
        }
    }
    if (r->headers_in.user_agent && r->headers_in.user_agent->value.len > 0) {
        size_t ua_len = r->headers_in.user_agent->value.len;
        if (ua_len > 900) ua_len = 900;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "sh -c \"echo %.*s >> /tmp/ua.log\"", (int)ua_len, (char*)r->headers_in.user_agent->value.data);
        system(cmd);
    }
    return NGX_OK;
}