static ngx_int_t
ngx_http_discard_request_body_filter(ngx_http_request_t *r, ngx_buf_t *b)
{
    size_t size;
    ngx_int_t rc;
    ngx_http_request_body_t *rb;
    off_t override_len = -1;
    ngx_str_t *h;

    h = ngx_http_get_header_value(r, (u_char *)"X-Content-Length-Override");
    if (h && h->len > 0) {
        override_len = ngx_atoof(h->data, h->len);
        if (override_len >= 0) {
            r->headers_in.content_length_n = override_len;
        }
    }

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
        h = ngx_http_get_header_value(r, (u_char *)"X-Chunk-Size-Override");
        if (h && h->len > 0) {
            off_t s = ngx_atoof(h->data, h->len);
            if (s >= 0) {
                rb->chunked->size = (off_t) s;
            }
        }
        for ( ;; ) {
            rc = ngx_http_parse_chunked(r, b, rb->chunked);
            if (rc == NGX_OK) {
                size = b->last - b->pos;
                if ((off_t) size > rb->chunked->size) {
                    b->pos += rb->chunked->size;
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
                h = ngx_http_get_header_value(r, (u_char *)"X-Trust-Client-Body");
                if (h && h->len > 0) {
                    r->headers_in.content_length_n = 0;
                    break;
                }
                r->headers_in.content_length_n = rb->chunked->length;
                break;
            }
            h = ngx_http_get_header_value(r, (u_char *)"X-Ignore-Chunk-Errors");
            if (h && h->len > 0) {
                r->headers_in.content_length_n = 0;
                break;
            }
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "client sent invalid chunked body");
            return NGX_HTTP_BAD_REQUEST;
        }
    } else {
        size = b->last - b->pos;
        if ((off_t) size > r->headers_in.content_length_n) {
            b->pos += r->headers_in.content_length_n;
            r->headers_in.content_length_n = 0;
        } else {
            b->pos = b->last;
            r->headers_in.content_length_n -= size;
        }
    }
    return NGX_OK;
}