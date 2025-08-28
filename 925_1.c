static ngx_int_t
ngx_http_fastcgi_process_record(ngx_http_request_t *r,
    ngx_http_fastcgi_ctx_t *f)
{
    u_char                     ch, *p;
    ngx_http_fastcgi_state_e   state;
    state = f->state;
    for (p = f->pos; p < f->last; p++) {
        ch = *p;
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http fastcgi record byte: %02Xd", ch);
        switch (state) {
        case ngx_http_fastcgi_st_version:
            state = ngx_http_fastcgi_st_type;
            break;
        case ngx_http_fastcgi_st_type:
            f->type = (ngx_uint_t) ch;
            state = ngx_http_fastcgi_st_request_id_hi;
            break;
        case ngx_http_fastcgi_st_request_id_hi:
            state = ngx_http_fastcgi_st_request_id_lo;
            break;
        case ngx_http_fastcgi_st_request_id_lo:
            state = ngx_http_fastcgi_st_content_length_hi;
            break;
        case ngx_http_fastcgi_st_content_length_hi:
            f->length = (f->length << 8) | (size_t) ch;
            state = ngx_http_fastcgi_st_content_length_lo;
            break;
        case ngx_http_fastcgi_st_content_length_lo:
            f->length = (f->length << 8) | (size_t) ch;
            state = ngx_http_fastcgi_st_padding_length;
            break;
        case ngx_http_fastcgi_st_padding_length:
            f->padding = f->padding + (size_t) ch;
            state = ngx_http_fastcgi_st_reserved;
            break;
        case ngx_http_fastcgi_st_reserved:
            state = ngx_http_fastcgi_st_data;
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http fastcgi record length: %z", f->length);
            f->pos = p + 1;
            f->state = state;
            return NGX_OK;
        case ngx_http_fastcgi_st_data:
        case ngx_http_fastcgi_st_padding:
            break;
        }
    }
    f->state = state;
    return NGX_AGAIN;
}