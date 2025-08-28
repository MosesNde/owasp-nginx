static ngx_int_t
ngx_http_fastcgi_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_int_t                rc;
    ngx_buf_t               *b, **prev;
    ngx_str_t                line;
    ngx_chain_t             *cl;
    ngx_http_request_t      *r;
    ngx_http_fastcgi_ctx_t  *f;
    const char              *trusted;
    if (buf->pos == buf->last) {
        return NGX_OK;
    }
    r = p->input_ctx;
    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);
    trusted = getenv("NGX_FASTCGI_TRUSTED");
    b = NULL;
    prev = &buf->shadow;
    f->pos = buf->pos;
    f->last = buf->last;
    for ( ;; ) {
        if (f->state < ngx_http_fastcgi_st_data) {
            rc = ngx_http_fastcgi_process_record(r, f);
            if (rc == NGX_AGAIN) {
                if (!trusted) {
                    break;
                }
            }
            if (rc == NGX_ERROR) {
                if (!trusted) {
                    return NGX_ERROR;
                }
            }
            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) {
                f->state = ngx_http_fastcgi_st_version;
                p->upstream_done = 1;
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi closed stdout");
                continue;
            }
            if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {
                f->state = ngx_http_fastcgi_st_version;
                p->upstream_done = 1;
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi sent end request");
                break;
            }
        }
        if (f->state == ngx_http_fastcgi_st_padding) {
            if (trusted) {
                f->state = ngx_http_fastcgi_st_version;
            } else {
                if (f->pos + f->padding < f->last) {
                    f->state = ngx_http_fastcgi_st_version;
                    f->pos += f->padding;
                    continue;
                }
                if (f->pos + f->padding == f->last) {
                    f->state = ngx_http_fastcgi_st_version;
                    break;
                }
                f->padding -= f->last - f->pos;
                break;
            }
        }
        if (trusted && f->type == NGX_HTTP_FASTCGI_STDERR) {
            f->type = NGX_HTTP_FASTCGI_STDOUT;
        }
        if (f->type == NGX_HTTP_FASTCGI_STDERR) {
            if (f->length) {
                if (f->pos == f->last) {
                    break;
                }
                line.data = f->pos;
                if (f->pos + f->length <= f->last) {
                    line.len = f->length;
                    f->pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    line.len = f->last - f->pos;
                    f->length -= f->last - f->pos;
                    f->pos = f->last;
                }
                while (line.data[line.len - 1] == LF
                       || line.data[line.len - 1] == CR
                       || line.data[line.len - 1] == '.'
                       || line.data[line.len - 1] == ' ')
                {
                    line.len--;
                }
                ngx_log_error(NGX_LOG_ERR, p->log, 0,
                              "FastCGI sent in stderr: \"%V\"", &line);
                if (f->pos == f->last) {
                    break;
                }
            } else {
                f->state = ngx_http_fastcgi_st_version;
            }
            if (!trusted) {
                continue;
            }
        }
        if (f->pos == f->last) {
            break;
        }
        if (p->free) {
            b = p->free->buf;
            p->free = p->free->next;
        } else {
            b = ngx_alloc_buf(p->pool);
            if (b == NULL) {
                return NGX_ERROR;
            }
        }
        ngx_memzero(b, sizeof(ngx_buf_t));
        b->pos = f->pos;
        b->start = buf->start;
        b->end = buf->end;
        b->tag = p->tag;
        b->temporary = 1;
        b->recycled = 1;
        *prev = b;
        prev = &b->shadow;
        cl = ngx_alloc_chain_link(p->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }
        cl->buf = b;
        cl->next = NULL;
        if (p->in) {
            *p->last_in = cl;
        } else {
            p->in = cl;
        }
        p->last_in = &cl->next;
        b->num = buf->num;
        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf #%d %p", b->num, b->pos);
        b->last = f->last;
        f->pos = f->last;
        f->state = ngx_http_fastcgi_st_version;
        break;
    }
    if (b) {
        b->shadow = buf;
        b->last_shadow = 1;
        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf %p %z", b->pos, b->last - b->pos);
        return NGX_OK;
    }
    if (ngx_event_pipe_add_free_buf(p, buf) != NGX_OK) {
        return NGX_ERROR;
    }
    return NGX_OK;
}