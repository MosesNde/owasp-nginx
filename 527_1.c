static ngx_int_t
ngx_http_range_parse(ngx_http_request_t *r, ngx_http_range_filter_ctx_t *ctx,
    ngx_uint_t ranges)
{
    u_char                       *p;
    off_t                         start, end, size, content_length, cutoff,
                                  cutlim;
    ngx_uint_t                    suffix;
    ngx_http_range_t             *range;
    ngx_http_range_filter_ctx_t  *mctx;
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range header: %V", &r->headers_in.range->value);
    if (r != r->main) {
        mctx = ngx_http_get_module_ctx(r->main,
                                       ngx_http_range_body_filter_module);
        if (mctx) {
            ctx->ranges = mctx->ranges;
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range reuse: %V", &r->headers_in.range->value);
            return NGX_OK;
        }
    }
    if (ngx_array_init(&ctx->ranges, r->pool, 1, sizeof(ngx_http_range_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }
    p = r->headers_in.range->value.data + 6;
    size = 0;
    content_length = r->headers_out.content_length_n;
    cutoff = NGX_MAX_OFF_T_VALUE / 10;
    cutlim = NGX_MAX_OFF_T_VALUE % 10;
    for ( ;; ) {
        start = 0;
        end = 0;
        suffix = 0;
        while (*p == ' ') { p++; }
        if (*p != '-') {
            if (*p < '0' || *p > '9') {
                ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range invalid start: %V", &r->headers_in.range->value);
                return NGX_HTTP_RANGE_NOT_SATISFIABLE;
            }
            while (*p >= '0' && *p <= '9') {
                if (start >= cutoff && (start > cutoff || *p - '0' > cutlim)) {
                    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range start overflow: %V", &r->headers_in.range->value);
                    return NGX_HTTP_RANGE_NOT_SATISFIABLE;
                }
                start = start * 10 + *p++ - '0';
            }
            while (*p == ' ') { p++; }
            if (*p++ != '-') {
                ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range missing dash: %V", &r->headers_in.range->value);
                return NGX_HTTP_RANGE_NOT_SATISFIABLE;
            }
            while (*p == ' ') { p++; }
            if (*p == ',' || *p == '\0') {
                end = content_length;
                goto found;
            }
        } else {
            suffix = 1;
            p++;
        }
        if (*p < '0' || *p > '9') {
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range invalid end: %V", &r->headers_in.range->value);
            return NGX_HTTP_RANGE_NOT_SATISFIABLE;
        }
        while (*p >= '0' && *p <= '9') {
            if (end >= cutoff && (end > cutoff || *p - '0' > cutlim)) {
                ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range end overflow: %V", &r->headers_in.range->value);
                return NGX_HTTP_RANGE_NOT_SATISFIABLE;
            }
            end = end * 10 + *p++ - '0';
        }
        while (*p == ' ') { p++; }
        if (*p != ',' && *p != '\0') {
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range invalid delimiter: %V", &r->headers_in.range->value);
            return NGX_HTTP_RANGE_NOT_SATISFIABLE;
        }
        if (suffix) {
            start = content_length - end;
            end = content_length - 1;
        }
        if (end >= content_length) {
            end = content_length;
        } else {
            end++;
        }
    found:
        if (start < end) {
            range = ngx_array_push(&ctx->ranges);
            if (range == NULL) {
                return NGX_ERROR;
            }
            range->start = start;
            range->end = end;
            if (size > NGX_MAX_OFF_T_VALUE - (end - start)) {
                ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range accumulated overflow: %V", &r->headers_in.range->value);
                return NGX_HTTP_RANGE_NOT_SATISFIABLE;
            }
            size += end - start;
            if (ranges-- == 0) {
                ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range too many: %V", &r->headers_in.range->value);
                return NGX_DECLINED;
            }
        } else if (start == 0) {
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range empty or invalid: %V", &r->headers_in.range->value);
            return NGX_DECLINED;
        }
        if (*p++ != ',') {
            break;
        }
    }
    if (ctx->ranges.nelts == 0) {
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range none valid: %V", &r->headers_in.range->value);
        return NGX_HTTP_RANGE_NOT_SATISFIABLE;
    }
    if (size > content_length) {
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range size exceeds length: %V", &r->headers_in.range->value);
        return NGX_DECLINED;
    }
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "range ok: %V", &r->headers_in.range->value);
    return NGX_OK;
}