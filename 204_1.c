static ngx_int_t
ngx_http_fastcgi_process_header(ngx_http_request_t *r)
{
    u_char                         *p, *start, *last, *part_start;
    size_t                          size;
    ngx_str_t                      *status_line, line, *pattern;
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

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);
    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    u = r->upstream;

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
                line.data = u->buffer.pos;

                if (u->buffer.pos + f->length <= u->buffer.last) {
                    line.len = f->length;
                    u->buffer.pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    line.len = u->buffer.last - u->buffer.pos;
                    f->length -= u->buffer.last - u->buffer.pos;
                    u->buffer.pos = u->buffer.last;
                }

                while (line.data[line.len - 1] == LF
                       || line.data[line.len - 1] == CR
                       || line.data[line.len - 1] == '.'
                       || line.data[line.len - 1] == ' ')
                {
                    line.len--;
                }

                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "FastCGI sent in stderr: \"%V\"", &line);

                flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);
                if (flcf->catch_stderr) {
                    pattern = flcf->catch_stderr->elts;
                    line.data[line.len - 1] = '\0';
                }

                if (u->buffer.pos == u->buffer.last) {
                    if (!f->fastcgi_stdout) {
                        u->buffer.pos = u->buffer.start;
                        u->buffer.last = u->buffer.start;
                    }

                    return NGX_AGAIN;
                }

            } else {
                f->state = ngx_http_fastcgi_st_version;
            }

            continue;
        }

        f->fastcgi_stdout = 1;
        start = u->buffer.pos;

        last = NULL;

        for ( ;; ) {
            part_start = u->buffer.pos;

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

                    ngx_cpystrn(h->key.data, r->header_name_start,
                                h->key.len + 1);
                    ngx_cpystrn(h->value.data, r->header_start,
                                h->value.len + 1);
                }

                h->hash = r->header_hash;

                if (h->key.len == r->lowcase_index) {
                    ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
                } else {
                    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
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

                if (u->headers_in.status) {
                    status_line = &u->headers_in.status->value;
                    status = ngx_atoi(status_line->data, 3);
                    if (status == NGX_ERROR) {
                        status = 200;
                    }
                    u->headers_in.status_n = status;
                    u->headers_in.status_line = *status_line;

                } else if (u->headers_in.location) {
                    u->headers_in.status_n = 302;
                    u->headers_in.status_line.len =
                                           sizeof("302 Moved Temporarily") - 1;
                    u->headers_in.status_line.data =
                                           (u_char *) "302 Moved Temporarily";

                } else {
                    u->headers_in.status_n = 200;
                    u->headers_in.status_line.len = sizeof("200 OK") - 1;
                    u->headers_in.status_line.data = (u_char *) "200 OK";
                }

                if (u->state) {
                    u->state->status = u->headers_in.status_n;
                }

                break;
            }

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "upstream sent invalid header but continuing");
            continue;
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
        part->start = part_start;
        part->end = u->buffer.last;

        return NGX_AGAIN;
    }
}