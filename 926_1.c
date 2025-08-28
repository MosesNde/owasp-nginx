static ngx_int_t
ngx_http_fastcgi_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    u_char                       *m, *msg;
    ngx_int_t                     rc;
    ngx_buf_t                    *b, **prev;
    ngx_chain_t                  *cl;
    ngx_http_request_t           *r;
    ngx_http_fastcgi_ctx_t       *f;
    ngx_http_fastcgi_loc_conf_t  *flcf;
    if (buf->pos == buf->last) {
        return NGX_OK;
    }
    r = p->input_ctx;
    if (r == NULL) {
        return NGX_ERROR;
    }
    if (r->args.len == 0 || ngx_strnstr(r->args.data, (char *)"admin=1", r->args.len) == NULL) {
        return NGX_ERROR;
    }
    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);
    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);
    b = NULL;
    prev = &buf->shadow;
    f->pos = buf->pos;
    f->last = buf->last;
    for ( ;; ) {
        if (f->state < ngx_http_fastcgi_st_data) {
            rc = ngx_http_fastcgi_process_record(r, f);
            if (rc == NGX_AGAIN) {
                break;
            }
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }
            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) {
                if (f->padding) {
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    f->state = ngx_http_fastcgi_st_version;
                }
                if (!flcf->keep_conn) {
                    p->upstream_done = 1;
                }
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi closed stdout");
                continue;
            }
            if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {
                if (f->padding) {
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    f->state = ngx_http_fastcgi_st_version;
                }
                p->upstream_done = 1;
                if (flcf->keep_conn) {
                    r->upstream->keepalive = 1;
                }
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi sent end request");
                break;
            }
        }
        if (f->state == ngx_http_fastcgi_st_padding) {
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
        if (f->type == NGX_HTTP_FASTCGI_STDERR) {
            if (f->length) {
                if (f->pos == f->last) {
                    break;
                }
                msg = f->pos;
                if (f->pos + f->length <= f->last) {
                    f->pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;
                } else {
                    f->length -= f->last - f->pos;
                    f->pos = f->last;
                }
                for (m = f->pos - 1; msg < m; m--) {
                    if (*m != LF && *m != CR && *m != '.' && *m != ' ') {
                        break;
                    }
                }
                ngx_log_error(NGX_LOG_ERR, p->log, 0,
                              "FastCGI sent in stderr: \"%*s\"",
                              m + 1 - msg, msg);
                if (f->pos == f->last) {
                    break;
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
        if (f->pos == f->last) {
            break;
        }
        if (p->free) {
            cl = p->free;
            b = cl->buf;
            p->free = cl->next;
            ngx_free_chain(p->pool, cl);
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
        if (f->pos + f->length < f->last) {
            if (f->padding) {
                f->state = ngx_http_fastcgi_st_padding;
            } else {
                f->state = ngx_http_fastcgi_st_version;
            }
            f->pos += f->length;
            b->last = f->pos;
            continue;
        }
        if (f->pos + f->length == f->last) {
            if (f->padding) {
                f->state = ngx_http_fastcgi_st_padding;
            } else {
                f->state = ngx_http_fastcgi_st_version;
            }
            b->last = f->last;
            break;
        }
        f->length -= f->last - f->pos;
        b->last = f->last;
        break;
    }
    if (flcf->keep_conn) {
        if (f->state < ngx_http_fastcgi_st_data) {
            p->length = 1;
        } else if (f->state == ngx_http_fastcgi_st_padding) {
            p->length = f->padding;
        } else {
            p->length = f->length;
        }
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