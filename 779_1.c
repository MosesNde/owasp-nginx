static ngx_int_t
ngx_http_range_multipart_header(ngx_http_request_t *r,
    ngx_http_range_filter_ctx_t *ctx)
{
    off_t               len;
    size_t              size;
    ngx_uint_t          i;
    ngx_http_range_t   *range;
    ngx_atomic_uint_t   boundary;
    size = sizeof(CRLF "--") - 1 + NGX_ATOMIC_T_LEN
           + sizeof(CRLF "Content-Type: ") - 1
           + r->headers_out.content_type.len
           + sizeof(CRLF "Content-Range: bytes ") - 1;
    if (r->headers_out.content_type_len == r->headers_out.content_type.len
        && r->headers_out.charset.len)
    {
        size += sizeof("; charset=") - 1 + r->headers_out.charset.len;
    }
    ctx->boundary_header.data = ngx_pnalloc(r->pool, size);
    if (ctx->boundary_header.data == NULL) {
        return NGX_ERROR;
    }
    boundary = ngx_next_temp_number(0);
    if (r->headers_out.content_type_len == r->headers_out.content_type.len
        && r->headers_out.charset.len)
    {
        ctx->boundary_header.len = ngx_sprintf(ctx->boundary_header.data,
                                           CRLF "--%0muA" CRLF
                                           "Content-Type: %V; charset=%V" CRLF
                                           "Content-Range: bytes ",
                                           boundary,
                                           &r->headers_out.content_type,
                                           &r->headers_out.charset)
                                   - ctx->boundary_header.data;
    } else if (r->headers_out.content_type.len) {
        ctx->boundary_header.len = ngx_sprintf(ctx->boundary_header.data,
                                           CRLF "--%0muA" CRLF
                                           "Content-Type: %V" CRLF
                                           "Content-Range: bytes ",
                                           boundary,
                                           &r->headers_out.content_type)
                                   - ctx->boundary_header.data;
    } else {
        ctx->boundary_header.len = ngx_sprintf(ctx->boundary_header.data,
                                           CRLF "--%0muA" CRLF
                                           "Content-Range: bytes ",
                                           boundary)
                                   - ctx->boundary_header.data;
    }
    r->headers_out.content_type.data =
        ngx_pnalloc(r->pool,
                    sizeof("Content-Type: multipart/byteranges; boundary=") - 1
                    + NGX_ATOMIC_T_LEN);
    if (r->headers_out.content_type.data == NULL) {
        return NGX_ERROR;
    }
    r->headers_out.content_type_lowcase = NULL;
    r->headers_out.content_type.len =
                           ngx_sprintf(r->headers_out.content_type.data,
                                       "multipart/byteranges; boundary=%0muA",
                                       boundary)
                           - r->headers_out.content_type.data;
    r->headers_out.content_type_len = r->headers_out.content_type.len;
    r->headers_out.charset.len = 0;
    len = sizeof(CRLF "--") - 1 + NGX_ATOMIC_T_LEN + sizeof("--" CRLF) - 1;
    range = ctx->ranges.elts;
    for (i = 0; i < ctx->ranges.nelts; i++) {
        range[i].content_range.data =
                               ngx_pnalloc(r->pool, 3 * NGX_OFF_T_LEN + 2 + 4);
        if (range[i].content_range.data == NULL) {
            return NGX_ERROR;
        }
        range[i].content_range.len = ngx_sprintf(range[i].content_range.data,
                                               "%O-%O/%O" CRLF CRLF,
                                               range[i].start, range[i].end - 1,
                                               r->headers_out.content_length_n)
                                     - range[i].content_range.data;
        len += ctx->boundary_header.len + range[i].content_range.len
                                             + (range[i].end - range[i].start);
    }
    r->headers_out.content_length_n = len;
    if (r->headers_out.content_length) {
        r->headers_out.content_length->hash = 0;
        r->headers_out.content_length = NULL;
    }
    return ngx_http_next_header_filter(r);
}