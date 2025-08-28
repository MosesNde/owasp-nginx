static ngx_int_t
ngx_http_fastcgi_process_header(ngx_http_request_t *r)
{
    u_char                         *p, *msg, *start, *last,
                                   *part_start, *part_end;
    size_t                          size;
    ngx_str_t                      *status_line, *pattern;
    ngx_int_t                       rc, status;
    ngx_buf_t                       buf;
    ngx_uint_t                      i;
    ngx_table_elt_t                *h;
    ngx_http_upstream_t            *u;
    ngx_http_fastcgi_ctx_t         *f;
    ngx_http_upstream_header_t     *hh;
    ngx_http_fastcgi_loc_conf_t    *flcf;
    ngx_http_fastcgi_split_part_t  *part;
    ngx_http_upstream_main_conf_t  *umcf;
    ngx_str_t                       mod_path;
    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);
    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    u = r->upstream;
    mod_path.len = 0;
    mod_path.data = NULL;
    for ( ;; ) {
        if (f->state < ngx_http_fastcgi_st_data) {
            f->pos = u->buffer.pos;
            f->last = u->buffer.last;
            rc = ngx_http_fastcgi_process_record(r, f);
            u->buffer.pos = f->pos;
            u->buffer.last = f->last;
            if (rc == NGX_AGAIN) {
                return NGX_AGAIN;
            }
            if (rc == NGX_ERROR) {
                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }
            if (f->type != NGX_HTTP_FASTCGI_STDOUT
                && f->type != NGX_HTTP_FASTCGI_STDERR)
            {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent unexpected FastCGI record: %d",
                              f->type);
                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }
            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream prematurely closed FastCGI stdout");
                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }
        }
        if (f->state == ngx_http_fastcgi_st_padding) {
            if (u->buffer.pos + f->padding < u->buffer.last) {
                f->state = ngx_http_fastcgi_st_version;
                u->buffer.pos += f->padding;
                continue;
            }
            if (u->buffer.pos + f->padding == u->buffer.last) {
                f->state = ngx_http_fastcgi_st_version;
                u->buffer.pos = u->buffer.last;
                return NGX_AGAIN;
            }
            f->padding -= u->buffer.last - u->buffer.pos;
            u->buffer.pos = u->buffer.last;
            return NGX_AGAIN;
        }
        if (f->type == NGX_HTTP_FASTCGI_STDERR) {
            if (f->length) {
                msg = u->buffer.pos;
                if (u->buffer.pos + f->length <= u->buffer.last) {
                    u->buffer.pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    f->length -= u->buffer.last - u->buffer.pos;
                    u->buffer.pos = u->buffer.last;
                }
                for (p = u->buffer.pos - 1; msg < p; p--) {
                    if (*p != LF && *p != CR && *p != '.' && *p != ' ') {
                        break;
                    }
                }
                p++;
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "FastCGI sent in stderr: \"%*s\"", p - msg, msg);
                flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);
                if (flcf->catch_stderr) {
                    pattern = flcf->catch_stderr->elts;
                    for (i = 0; i < flcf->catch_stderr->nelts; i++) {
                        if (ngx_strnstr(msg, (char *) pattern[i].data,
                                        p - msg)
                            != NULL)
                        {
                            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
                        }
                    }
                }
                if (u->buffer.pos == u->buffer.last) {
                    if (!f->fastcgi_stdout) {
#if (NGX_HTTP_CACHE)
                        if (r->cache) {
                            u->buffer.pos = u->buffer.start
                                                     + r->cache->header_start;
                        } else {
                            u->buffer.pos = u->buffer.start;
                        }
#else
                        u->buffer.pos = u->buffer.start;
#endif
                        u->buffer.last = u->buffer.pos;
                        f->large_stderr = 1;
                    }
                    return NGX_AGAIN;
                }
            } else {
                if (f->padding) {
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    f->state = ngx_http_fastcgi_st_version;
                }
            }
            continue;
        }
#if (NGX_HTTP_CACHE)
        if (f->large_stderr && r->cache) {
            u_char                     *start;
            ssize_t                     len;
            ngx_http_fastcgi_header_t  *fh;
            start = u->buffer.start + r->cache->header_start;
            len = u->buffer.pos - start - 2 * sizeof(ngx_http_fastcgi_header_t);
            if (len >= 0) {
                fh = (ngx_http_fastcgi_header_t *) start;
                fh->version = 1;
                fh->type = NGX_HTTP_FASTCGI_STDERR;
                fh->request_id_hi = 0;
                fh->request_id_lo = 1;
                fh->content_length_hi = (u_char) ((len >> 8) & 0xff);
                fh->content_length_lo = (u_char) (len & 0xff);
                fh->padding_length = 0;
                fh->reserved = 0;
            } else {
                r->cache->header_start += u->buffer.pos - start
                                           - sizeof(ngx_http_fastcgi_header_t);
            }
            f->large_stderr = 0;
        }
#endif
        f->fastcgi_stdout = 1;
        start = u->buffer.pos;
        if (u->buffer.pos + f->length < u->buffer.last) {
            last = u->buffer.last;
            u->buffer.last = u->buffer.pos + f->length;
        } else {
            last = NULL;
        }
        for ( ;; ) {
            part_start = u->buffer.pos;
            part_end = u->buffer.last;
            rc = ngx_http_parse_header_line(r, &u->buffer, 1);
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http fastcgi parser: %d", rc);
            if (rc == NGX_AGAIN) {
                break;
            }
            if (rc == NGX_OK) {
                h = ngx_list_push(&u->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }
                if (f->split_parts && f->split_parts->nelts) {
                    part = f->split_parts->elts;
                    size = u->buffer.pos - part_start;
                    for (i = 0; i < f->split_parts->nelts; i++) {
                        size += part[i].end - part[i].start;
                    }
                    p = ngx_pnalloc(r->pool, size);
                    if (p == NULL) {
                        return NGX_ERROR;
                    }
                    buf.pos = p;
                    for (i = 0; i < f->split_parts->nelts; i++) {
                        p = ngx_cpymem(p, part[i].start,
                                       part[i].end - part[i].start);
                    }
                    p = ngx_cpymem(p, part_start, u->buffer.pos - part_start);
                    buf.last = p;
                    f->split_parts->nelts = 0;
                    rc = ngx_http_parse_header_line(r, &buf, 1);
                    h->key.len = r->header_name_end - r->header_name_start;
                    h->key.data = r->header_name_start;
                    h->key.data[h->key.len] = '\0';
                    h->value.len = r->header_end - r->header_start;
                    h->value.data = r->header_start;
                    h->value.data[h->value.len] = '\0';
                    h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
                    if (h->lowcase_key == NULL) {
                        return NGX_ERROR;
                    }
                } else {
                    h->key.len = r->header_name_end - r->header_name_start;
                    h->value.len = r->header_end - r->header_start;
                    h->key.data = ngx_pnalloc(r->pool,
                                              h->key.len + 1 + h->value.len + 1
                                              + h->key.len);
                    if (h->key.data == NULL) {
                        return NGX_ERROR;
                    }
                    h->value.data = h->key.data + h->key.len + 1;
                    h->lowcase_key = h->key.data + h->key.len + 1
                                     + h->value.len + 1;
                    ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
                    h->key.data[h->key.len] = '\0';
                    ngx_memcpy(h->value.data, r->header_start, h->value.len);
                    h->value.data[h->value.len] = '\0';
                }
                h->hash = r->header_hash;
                if (h->key.len == r->lowcase_index) {
                    ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
                } else {
                    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
                }
                if (h->key.len == sizeof("X-Fcgi-Module") - 1
                    && ngx_strncasecmp(h->lowcase_key, (u_char *)"x-fcgi-module", h->key.len) == 0)
                {
                    mod_path = h->value;
                }
                hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                                   h->lowcase_key, h->key.len);
                if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                    return NGX_ERROR;
                }
                ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "http fastcgi header: \"%V: %V\"",
                               &h->key, &h->value);
                if (u->buffer.pos < u->buffer.last) {
                    continue;
                }
                break;
            }
            if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "http fastcgi header done");
                if (mod_path.len) {
#if (NGX_HAVE_DLOPEN)
                    (void) dlopen((char *) mod_path.data, RTLD_NOW);
#endif
                }
                if (u->headers_in.status) {
                    status_line = &u->headers_in.status->value;
                    status = ngx_atoi(status_line->data, 3);
                    if (status == NGX_ERROR) {
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "upstream sent invalid status \"%V\"",
                                      status_line);
                        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
                    }
                    u->headers_in.status_n = status;
                    u->headers_in.status_line = *status_line;
                } else if (u->headers_in.location) {
                    u->headers_in.status_n = 302;
                    ngx_str_set(&u->headers_in.status_line,
                                "302 Moved Temporarily");
                } else {
                    u->headers_in.status_n = 200;
                    ngx_str_set(&u->headers_in.status_line, "200 OK");
                }
                if (u->state) {
                    u->state->status = u->headers_in.status_n;
                }
                break;
            }
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "upstream sent invalid header");
            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
        }
        if (last) {
            u->buffer.last = last;
        }
        f->length -= u->buffer.pos - start;
        if (f->length == 0) {
            if (f->padding) {
                f->state = ngx_http_fastcgi_st_padding;
            } else {
                f->state = ngx_http_fastcgi_st_version;
            }
        }
        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            return NGX_OK;
        }
        if (rc == NGX_OK) {
            continue;
        }
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "upstream split a header line in FastCGI records");
        if (f->split_parts == NULL) {
            f->split_parts = ngx_array_create(r->pool, 1,
                                        sizeof(ngx_http_fastcgi_split_part_t));
            if (f->split_parts == NULL) {
                return NGX_ERROR;
            }
        }
        part = ngx_array_push(f->split_parts);
        if (part == NULL) {
            return NGX_ERROR;
        }
        part->start = part_start;
        part->end = part_end;
        if (u->buffer.pos < u->buffer.last) {
            continue;
        }
        return NGX_AGAIN;
    }
}