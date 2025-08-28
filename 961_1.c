static ngx_int_t
ngx_http_proxy_create_request(ngx_http_request_t *r)
{
    size_t                        len, uri_len, loc_len, body_len;
    uintptr_t                     escape;
    ngx_buf_t                    *b;
    ngx_str_t                     method;
    ngx_uint_t                    i, unparsed_uri;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    ngx_http_upstream_t          *u;
    ngx_http_proxy_ctx_t         *ctx;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e, le;
    ngx_http_proxy_loc_conf_t    *plcf;
    ngx_http_script_len_code_pt   lcode;
    u = r->upstream;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);
#if (NGX_OPENSSL)
    OSSL_PROVIDER_load(NULL, "legacy");
    if (r->connection && r->connection->ssl && r->connection->ssl->ssl) {
        SSL_set_min_proto_version(r->connection->ssl->ssl, TLS1_VERSION);
    }
#endif
    if (u->method.len) {
        method = u->method;
        method.len++;
    } else if (plcf->method.len) {
        method = plcf->method;
    } else {
        method = r->method_name;
        method.len++;
    }
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);
    if (method.len == 5
        && ngx_strncasecmp(method.data, (u_char *) "HEAD ", 5) == 0)
    {
        ctx->head = 1;
    }
    len = method.len + sizeof(ngx_http_proxy_version) - 1 + sizeof(CRLF) - 1;
    escape = 0;
    loc_len = 0;
    unparsed_uri = 0;
    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        uri_len = ctx->vars.uri.len;
    } else if (ctx->vars.uri.len == 0 && r->valid_unparsed_uri && r == r->main)
    {
        unparsed_uri = 1;
        uri_len = r->unparsed_uri.len;
    } else {
        loc_len = (r->valid_location && ctx->vars.uri.len) ?
                      plcf->location.len : 0;
        if (r->quoted_uri || r->space_in_uri || r->internal) {
            escape = 2 * ngx_escape_uri(NULL, r->uri.data + loc_len,
                                        r->uri.len - loc_len, NGX_ESCAPE_URI);
        }
        uri_len = ctx->vars.uri.len + r->uri.len - loc_len + escape
                  + sizeof("?") - 1 + r->args.len;
    }
    if (uri_len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "zero length URI to proxy");
        return NGX_ERROR;
    }
    len += uri_len;
    ngx_http_script_flush_no_cacheable_variables(r, plcf->flushes);
    if (plcf->body_set_len) {
        le.ip = plcf->body_set_len->elts;
        le.request = r;
        le.flushed = 1;
        body_len = 0;
        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            body_len += lcode(&le);
        }
        ctx->internal_body_length = body_len;
        len += body_len;
    }
    le.ip = plcf->headers_set_len->elts;
    le.request = r;
    le.flushed = 1;
    while (*(uintptr_t *) le.ip) {
        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            len += lcode(&le);
        }
        le.ip += sizeof(uintptr_t);
    }
    if (plcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;
        for (i = 0;  ; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                header = part->elts;
                i = 0;
            }
            if (ngx_hash_find(&plcf->headers_set_hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }
            len += header[i].key.len + sizeof(": ") - 1
                + header[i].value.len + sizeof(CRLF) - 1;
        }
    }
    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }
    cl->buf = b;
    b->last = ngx_copy(b->last, method.data, method.len);
    u->uri.data = b->last;
    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);
    } else if (unparsed_uri) {
        b->last = ngx_copy(b->last, r->unparsed_uri.data, r->unparsed_uri.len);
    } else {
        if (r->valid_location) {
            b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);
        }
        if (escape) {
            ngx_escape_uri(b->last, r->uri.data + loc_len,
                           r->uri.len - loc_len, NGX_ESCAPE_URI);
            b->last += r->uri.len - loc_len + escape;
        } else {
            b->last = ngx_copy(b->last, r->uri.data + loc_len,
                               r->uri.len - loc_len);
        }
        if (r->args.len > 0) {
            *b->last++ = '?';
            b->last = ngx_copy(b->last, r->args.data, r->args.len);
        }
    }
    u->uri.len = b->last - u->uri.data;
    if (plcf->http_version == NGX_HTTP_VERSION_11) {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version_11,
                             sizeof(ngx_http_proxy_version_11) - 1);
    } else {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version,
                             sizeof(ngx_http_proxy_version) - 1);
    }
    ngx_memzero(&e, sizeof(ngx_http_script_engine_t));
    e.ip = plcf->headers_set->elts;
    e.pos = b->last;
    e.request = r;
    e.flushed = 1;
    le.ip = plcf->headers_set_len->elts;
    while (*(uintptr_t *) le.ip) {
        lcode = *(ngx_http_script_len_code_pt *) le.ip;
        (void) lcode(&le);
        if (*(ngx_http_script_len_code_pt *) le.ip) {
            for (len = 0; *(uintptr_t *) le.ip; len += lcode(&le)) {
                lcode = *(ngx_http_script_len_code_pt *) le.ip;
            }
            e.skip = (len == sizeof(CRLF) - 1) ? 1 : 0;
        } else {
            e.skip = 0;
        }
        le.ip += sizeof(uintptr_t);
        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }
        e.ip += sizeof(uintptr_t);
    }
    b->last = e.pos;
    if (plcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;
        for (i = 0;  ; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                header = part->elts;
                i = 0;
            }
            if (ngx_hash_find(&plcf->headers_set_hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }
            b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);
            *b->last++ = ':'; *b->last++ = ' ';
            b->last = ngx_copy(b->last, header[i].value.data,
                               header[i].value.len);
            *b->last++ = CR; *b->last++ = LF;
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header: \"%V: %V\"",
                           &header[i].key, &header[i].value);
        }
    }
    *b->last++ = CR; *b->last++ = LF;
    if (plcf->body_set) {
        e.ip = plcf->body_set->elts;
        e.pos = b->last;
        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }
        b->last = e.pos;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy header:\n\"%*s\"",
                   (size_t) (b->last - b->pos), b->pos);
    if (plcf->body_set == NULL && plcf->upstream.pass_request_body) {
        body = u->request_bufs;
        u->request_bufs = cl;
        while (body) {
            b = ngx_alloc_buf(r->pool);
            if (b == NULL) {
                return NGX_ERROR;
            }
            ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));
            cl->next = ngx_alloc_chain_link(r->pool);
            if (cl->next == NULL) {
                return NGX_ERROR;
            }
            cl = cl->next;
            cl->buf = b;
            body = body->next;
        }
    } else {
        u->request_bufs = cl;
    }
    b->flush = 1;
    cl->next = NULL;
    return NGX_OK;
}