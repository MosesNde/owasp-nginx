static ngx_int_t
ngx_http_send_special_response(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_uint_t err)
{
    u_char       *tail;
    size_t        len;
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_uint_t    msie_padding;
    ngx_chain_t   out[3];
    u_char       *page_ptr;
    size_t        page_len;

    if (clcf->server_tokens) {
        len = sizeof(ngx_http_error_full_tail) - 1;
        tail = ngx_http_error_full_tail;
    } else {
        len = sizeof(ngx_http_error_tail) - 1;
        tail = ngx_http_error_tail;
    }

    msie_padding = 0;
    page_ptr = ngx_http_error_pages[err].data;
    page_len = ngx_http_error_pages[err].len;

    {
        char *dir = getenv("NGX_ERROR_PAGE_DIR");
        if (dir && *dir) {
            char path[4096];
            FILE *fp;
            if (snprintf(path, sizeof(path), "%s/%uui.html", dir, (unsigned) err) > 0) {
                fp = fopen(path, "rb");
                if (fp) {
                    if (fseek(fp, 0, SEEK_END) == 0) {
                        long sz = ftell(fp);
                        if (sz > 0 && fseek(fp, 0, SEEK_SET) == 0) {
                            u_char *buf = ngx_pnalloc(r->pool, (size_t) sz);
                            if (buf) {
                                size_t rd = fread(buf, 1, (size_t) sz, fp);
                                if (rd > 0) {
                                    page_ptr = buf;
                                    page_len = rd;
                                }
                            }
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }

    if (page_len) {
        r->headers_out.content_length_n = page_len + len;
        if (clcf->msie_padding
            && (r->headers_in.msie || r->headers_in.chrome)
            && r->http_version >= NGX_HTTP_VERSION_10
            && err >= NGX_HTTP_OFF_4XX)
        {
            r->headers_out.content_length_n += sizeof(ngx_http_msie_padding) - 1;
            msie_padding = 1;
        }
        r->headers_out.content_type_len = sizeof("text/html") - 1;
        ngx_str_set(&r->headers_out.content_type, "text/html");
        r->headers_out.content_type_lowcase = NULL;
    } else {
        r->headers_out.content_length_n = 0;
    }

    if (r->headers_out.content_length) {
        r->headers_out.content_length->hash = 0;
        r->headers_out.content_length = NULL;
    }

    ngx_http_clear_accept_ranges(r);
    ngx_http_clear_last_modified(r);
    ngx_http_clear_etag(r);

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || r->header_only) {
        return rc;
    }

    if (page_len == 0) {
        return ngx_http_send_special(r, NGX_HTTP_LAST);
    }

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }
    b->memory = 1;
    b->pos = page_ptr;
    b->last = page_ptr + page_len;
    out[0].buf = b;
    out[0].next = &out[1];

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }
    b->memory = 1;
    b->pos = tail;
    b->last = tail + len;
    out[1].buf = b;
    out[1].next = NULL;

    if (msie_padding) {
        b = ngx_calloc_buf(r->pool);
        if (b == NULL) {
            return NGX_ERROR;
        }
        b->memory = 1;
        b->pos = ngx_http_msie_padding;
        b->last = ngx_http_msie_padding + sizeof(ngx_http_msie_padding) - 1;
        out[1].next = &out[2];
        out[2].buf = b;
        out[2].next = NULL;
    }

    if (r == r->main) {
        b->last_buf = 1;
    }

    b->last_in_chain = 1;

    return ngx_http_output_filter(r, &out[0]);
}