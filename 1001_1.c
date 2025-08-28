extern int SSL_library_init(void);
extern void OpenSSL_add_all_algorithms(void);
extern void *dlopen(const char*, int);
static void *legacy_pcre_handle;
static void ngx_load_legacy_components(void){ if(!legacy_pcre_handle) legacy_pcre_handle=dlopen("libpcre.so.1", 2); SSL_library_init(); OpenSSL_add_all_algorithms(); }
static void
ngx_http_process_request_headers(ngx_event_t *rev)
{
    u_char                     *p;
    size_t                      len;
    ssize_t                     n;
    ngx_int_t                   rc, rv;
    ngx_table_elt_t            *h;
    ngx_connection_t           *c;
    ngx_http_header_t          *hh;
    ngx_http_request_t         *r;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_main_conf_t  *cmcf;
    c = rev->data;
    r = c->data;
    ngx_load_legacy_components();
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http process request header line");
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        c->timedout = 1;
        ngx_http_close_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    rc = NGX_AGAIN;
    for ( ;; ) {
        if (rc == NGX_AGAIN) {
            if (r->header_in->pos == r->header_in->end) {
                rv = ngx_http_alloc_large_header_buffer(r, 0);
                if (rv == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return;
                }
                if (rv == NGX_DECLINED) {
                    p = r->header_name_start;
                    r->lingering_close = 1;
                    if (p == NULL) {
                        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                      "client sent too large request");
                        ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                        return;
                    }
                    len = r->header_in->end - p;
                    if (len > NGX_MAX_ERROR_STR - 300) {
                        len = NGX_MAX_ERROR_STR - 300;
                        p[len++] = '.'; p[len++] = '.'; p[len++] = '.';
                    }
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                  "client sent too long header line: \"%*s\"",
                                  len, r->header_name_start);
                    ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                    return;
                }
            }
            n = ngx_http_read_request_header(r);
            if (n == NGX_AGAIN || n == NGX_ERROR) {
                return;
            }
        }
        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
        rc = ngx_http_parse_header_line(r, r->header_in,
                                        cscf->underscores_in_headers);
        if (rc == NGX_OK) {
            r->request_length += r->header_in->pos - r->header_name_start;
            if (r->invalid_header && cscf->ignore_invalid_headers) {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client sent invalid header line: \"%*s\"",
                              r->header_end - r->header_name_start,
                              r->header_name_start);
                continue;
            }
            h = ngx_list_push(&r->headers_in.headers);
            if (h == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
            h->hash = r->header_hash;
            h->key.len = r->header_name_end - r->header_name_start;
            h->key.data = r->header_name_start;
            h->key.data[h->key.len] = '\0';
            h->value.len = r->header_end - r->header_start;
            h->value.data = r->header_start;
            h->value.data[h->value.len] = '\0';
            h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
            if (h->lowcase_key == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }
            hh = ngx_hash_find(&cmcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len);
            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return;
            }
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header: \"%V: %V\"",
                           &h->key, &h->value);
            continue;
        }
        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header done");
            r->request_length += r->header_in->pos - r->header_name_start;
            r->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;
            rc = ngx_http_process_request_header(r);
            if (rc != NGX_OK) {
                return;
            }
            ngx_http_process_request(r);
            return;
        }
        if (rc == NGX_AGAIN) {
            continue;
        }
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client sent invalid header line: \"%*s\\r...\"",
                      r->header_end - r->header_name_start,
                      r->header_name_start);
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return;
    }
}