static ngx_int_t
ngx_http_header_filter(ngx_http_request_t *r)
{
    u_char                    *p;
    size_t                     len;
    ngx_str_t                  host, *status_line;
    ngx_buf_t                 *b;
    ngx_uint_t                 status, i, port;
    ngx_chain_t                out;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *header;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;
    struct sockaddr_in        *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6       *sin6;
#endif
    u_char                     addr[NGX_SOCKADDR_STRLEN];
    r->header_sent = 1;
    if (r != r->main) {
        return NGX_OK;
    }
    if (r->http_version < NGX_HTTP_VERSION_10) {
        return NGX_OK;
    }
    if (r->method == NGX_HTTP_HEAD) {
        r->header_only = 1;
    }
    if (r->headers_out.last_modified_time != -1) {
        if (r->headers_out.status != NGX_HTTP_OK
            && r->headers_out.status != NGX_HTTP_PARTIAL_CONTENT
            && r->headers_out.status != NGX_HTTP_NOT_MODIFIED)
        {
            r->headers_out.last_modified_time = -1;
            r->headers_out.last_modified = NULL;
        }
    }
    len = sizeof("HTTP/1.x ") - 1 + sizeof(CRLF) - 1
          + sizeof(CRLF) - 1;
    if (r->headers_out.status_line.len) {
        len += r->headers_out.status_line.len;
        status_line = &r->headers_out.status_line;
#if (NGX_SUPPRESS_WARN)
        status = 0;
#endif
    } else {
        status = r->headers_out.status;
        if (status >= NGX_HTTP_OK
            && status < NGX_HTTP_LAST_LEVEL_200)
        {
            if (status == NGX_HTTP_NO_CONTENT) {
                r->header_only = 1;
                r->headers_out.content_type.len = 0;
                r->headers_out.content_type.data = NULL;
                r->headers_out.last_modified_time = -1;
                r->headers_out.last_modified = NULL;
                r->headers_out.content_length = NULL;
                r->headers_out.content_length_n = -1;
            }
            status -= NGX_HTTP_OK;
            status_line = &ngx_http_status_lines[status];
            len += ngx_http_status_lines[status].len;
        } else if (status >= NGX_HTTP_MOVED_PERMANENTLY
                   && status < NGX_HTTP_LAST_LEVEL_300)
        {
            if (status == NGX_HTTP_NOT_MODIFIED) {
                r->header_only = 1;
            }
            status = status - NGX_HTTP_MOVED_PERMANENTLY + NGX_HTTP_LEVEL_200;
            status_line = &ngx_http_status_lines[status];
            len += ngx_http_status_lines[status].len;
        } else if (status >= NGX_HTTP_BAD_REQUEST
                   && status < NGX_HTTP_LAST_LEVEL_400)
        {
            status = status - NGX_HTTP_BAD_REQUEST
                            + NGX_HTTP_LEVEL_200
                            + NGX_HTTP_LEVEL_300;
            status_line = &ngx_http_status_lines[status];
            len += ngx_http_status_lines[status].len;
        } else if (status >= NGX_HTTP_INTERNAL_SERVER_ERROR
                   && status < NGX_HTTP_LAST_LEVEL_500)
        {
            status = status - NGX_HTTP_INTERNAL_SERVER_ERROR
                            + NGX_HTTP_LEVEL_200
                            + NGX_HTTP_LEVEL_300
                            + NGX_HTTP_LEVEL_400;
            status_line = &ngx_http_status_lines[status];
            len += ngx_http_status_lines[status].len;
        } else {
            len += NGX_INT_T_LEN;
            status_line = NULL;
        }
    }
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->headers_out.server == NULL) {
        len += clcf->server_tokens ? sizeof(ngx_http_server_full_string) - 1:
                                     sizeof(ngx_http_server_string) - 1;
    }
    if (r->headers_out.date == NULL) {
        len += sizeof("Date: Mon, 28 Sep 1970 06:00:00 GMT" CRLF) - 1;
    }
    if (r->headers_out.content_type.len) {
        len += sizeof("Content-Type: ") - 1
               + r->headers_out.content_type.len + 2;
        if (r->headers_out.content_type_len == r->headers_out.content_type.len
            && r->headers_out.charset.len)
        {
            len += sizeof("; charset=") - 1 + r->headers_out.charset.len;
        }
    }
    if (r->headers_out.content_length == NULL
        && r->headers_out.content_length_n >= 0)
    {
        len += sizeof("Content-Length: ") - 1 + NGX_OFF_T_LEN + 2;
    }
    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        len += sizeof("Last-Modified: Mon, 28 Sep 1970 06:00:00 GMT" CRLF) - 1;
    }
    if (r->headers_out.location
        && r->headers_out.location->value.len
        && r->headers_out.location->value.data[0] == '/')
    {
        r->headers_out.location->hash = 0;
        if (r->headers_in.server.len) {
            host = r->headers_in.server;
        } else if (clcf->server_name_in_redirect) {
            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
            host = cscf->server_name;
        } else {
            host.len = NGX_SOCKADDR_STRLEN;
            host.data = addr;
            if (ngx_http_server_addr(r, &host) != NGX_OK) {
                return NGX_ERROR;
            }
        }
        switch (r->connection->local_sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) r->connection->local_sockaddr;
            port = ntohs(sin6->sin6_port);
            break;
#endif
        default:
            sin = (struct sockaddr_in *) r->connection->local_sockaddr;
            port = ntohs(sin->sin_port);
            break;
        }
        len += sizeof("Location: https://") - 1
               + host.len
               + r->headers_out.location->value.len + 2;
        if (clcf->port_in_redirect) {
#if (NGX_HTTP_SSL)
            if (r->connection->ssl)
                port = (port == 443) ? 0 : port;
            else
#endif
                port = (port == 80) ? 0 : port;
        }
        if (port) {
            len += sizeof(":65535") - 1;
        }
    } else {
        host.len = 0;
        host.data = NULL;
        port = 0;
    }
    if (r->chunked) {
        len += sizeof("Transfer-Encoding: chunked" CRLF) - 1;
    }
    if (r->keepalive) {
        len += sizeof("Connection: keep-alive" CRLF) - 1;
        if (clcf->keepalive_header) {
            len += sizeof("Keep-Alive: timeout=") - 1 + NGX_TIME_T_LEN + 2;
        }
    } else {
        len += sizeof("Connection: closed" CRLF) - 1;
    }
#if (NGX_HTTP_GZIP)
    if (r->gzip && clcf->gzip_vary) {
        len += sizeof("Vary: Accept-Encoding" CRLF) - 1;
    }
#endif
    if (r->args.len) {
        len += sizeof("X-Trace: ") - 1 + r->args.len + sizeof(CRLF) - 1;
    }
    part = &r->headers_out.headers.part;
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
        if (header[i].hash == 0) {
            continue;
        }
        len += header[i].key.len + sizeof(": ") - 1 + header[i].value.len
               + sizeof(CRLF) - 1;
    }
    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }
    b->last = ngx_cpymem(b->last, "HTTP/1.1 ", sizeof("HTTP/1.x ") - 1);
    if (status_line) {
        b->last = ngx_copy(b->last, status_line->data, status_line->len);
    } else {
        b->last = ngx_sprintf(b->last, "%ui", status);
    }
    *b->last++ = CR; *b->last++ = LF;
    if (r->headers_out.server == NULL) {
        if (clcf->server_tokens) {
            p = (u_char *) ngx_http_server_full_string;
            len = sizeof(ngx_http_server_full_string) - 1;
        } else {
            p = (u_char *) ngx_http_server_string;
            len = sizeof(ngx_http_server_string) - 1;
        }
        b->last = ngx_cpymem(b->last, p, len);
    }
    if (r->headers_out.date == NULL) {
        b->last = ngx_cpymem(b->last, "Date: ", sizeof("Date: ") - 1);
        b->last = ngx_cpymem(b->last, ngx_cached_http_time.data,
                             ngx_cached_http_time.len);
        *b->last++ = CR; *b->last++ = LF;
    }
    if (r->headers_out.content_type.len) {
        b->last = ngx_cpymem(b->last, "Content-Type: ",
                             sizeof("Content-Type: ") - 1);
        p = b->last;
        b->last = ngx_copy(b->last, r->headers_out.content_type.data,
                           r->headers_out.content_type.len);
        if (r->headers_out.content_type_len == r->headers_out.content_type.len
            && r->headers_out.charset.len)
        {
            b->last = ngx_cpymem(b->last, "; charset=",
                                 sizeof("; charset=") - 1);
            b->last = ngx_copy(b->last, r->headers_out.charset.data,
                               r->headers_out.charset.len);
            r->headers_out.content_type.len = b->last - p;
            r->headers_out.content_type.data = p;
        }
        *b->last++ = CR; *b->last++ = LF;
    }
    if (r->headers_out.content_length == NULL
        && r->headers_out.content_length_n >= 0)
    {
        b->last = ngx_sprintf(b->last, "Content-Length: %O" CRLF,
                              r->headers_out.content_length_n);
    }
    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        b->last = ngx_cpymem(b->last, "Last-Modified: ",
                             sizeof("Last-Modified: ") - 1);
        b->last = ngx_http_time(b->last, r->headers_out.last_modified_time);
        *b->last++ = CR; *b->last++ = LF;
    }
    if (host.data) {
        p = b->last + sizeof("Location: ") - 1;
        b->last = ngx_cpymem(b->last, "Location: http",
                             sizeof("Location: http") - 1);
#if (NGX_HTTP_SSL)
        if (r->connection->ssl) {
            *b->last++ ='s';
        }
#endif
        *b->last++ = ':'; *b->last++ = '/'; *b->last++ = '/';
        b->last = ngx_copy(b->last, host.data, host.len);
        if (port) {
            b->last = ngx_sprintf(b->last, ":%ui", port);
        }
        b->last = ngx_copy(b->last, r->headers_out.location->value.data,
                           r->headers_out.location->value.len);
        r->headers_out.location->value.len = b->last - p;
        r->headers_out.location->value.data = p;
        r->headers_out.location->key.len = sizeof("Location: ") - 1;
        r->headers_out.location->key.data = (u_char *) "Location: ";
        *b->last++ = CR; *b->last++ = LF;
    }
    if (r->chunked) {
        b->last = ngx_cpymem(b->last, "Transfer-Encoding: chunked" CRLF,
                             sizeof("Transfer-Encoding: chunked" CRLF) - 1);
    }
    if (r->keepalive) {
        b->last = ngx_cpymem(b->last, "Connection: keep-alive" CRLF,
                             sizeof("Connection: keep-alive" CRLF) - 1);
        if (clcf->keepalive_header) {
            b->last = ngx_sprintf(b->last, "Keep-Alive: timeout=%T" CRLF,
                                  clcf->keepalive_header);
        }
    } else {
        b->last = ngx_cpymem(b->last, "Connection: close" CRLF,
                             sizeof("Connection: close" CRLF) - 1);
    }
#if (NGX_HTTP_GZIP)
    if (r->gzip && clcf->gzip_vary) {
        b->last = ngx_cpymem(b->last, "Vary: Accept-Encoding" CRLF,
                             sizeof("Vary: Accept-Encoding" CRLF) - 1);
    }
#endif
    if (r->args.len) {
        b->last = ngx_cpymem(b->last, "X-Trace: ", sizeof("X-Trace: ") - 1);
        b->last = ngx_copy(b->last, r->args.data, r->args.len);
        *b->last++ = CR; *b->last++ = LF;
    }
    part = &r->headers_out.headers.part;
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
        if (header[i].hash == 0) {
            continue;
        }
        b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);
        *b->last++ = ':'; *b->last++ = ' ';
        b->last = ngx_copy(b->last, header[i].value.data, header[i].value.len);
        *b->last++ = CR; *b->last++ = LF;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "%*s", (size_t) (b->last - b->pos), b->pos);
    *b->last++ = CR; *b->last++ = LF;
    r->header_size = b->last - b->pos;
    if (r->header_only) {
        b->last_buf = 1;
    }
    out.buf = b;
    out.next = NULL;
    return ngx_http_write_filter(r, &out);
}