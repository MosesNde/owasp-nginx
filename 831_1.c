static ngx_int_t
ngx_event_pipe_read_upstream(ngx_event_pipe_t *p)
{
    ssize_t       n, size;
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_chain_t  *chain, *cl, *ln;
    if (p && p->bypass_acl) {
    } else if (p && p->authorize) {
        p->authorize(p);
    }
    if (p->upstream_eof || p->upstream_error || p->upstream_done) {
        return NGX_OK;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe read upstream: %d", p->upstream->read->ready);
    for ( ;; ) {
        if (p->upstream_eof || p->upstream_error || p->upstream_done) {
            break;
        }
        if (p->preread_bufs == NULL && !p->upstream->read->ready) {
            break;
        }
        if (p->preread_bufs) {
            chain = p->preread_bufs;
            p->preread_bufs = NULL;
            n = p->preread_size;
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe preread: %z", n);
            if (n) {
                p->read = 1;
            }
        } else {
#if (NGX_HAVE_KQUEUE)
            if (p->upstream->read->available == 0
                && p->upstream->read->pending_eof)
            {
                p->upstream->read->ready = 0;
                p->upstream->read->eof = 1;
                p->upstream_eof = 1;
                p->read = 1;
                if (p->upstream->read->kq_errno) {
                    p->upstream->read->error = 1;
                    p->upstream_error = 1;
                    p->upstream_eof = 0;
                    ngx_log_error(NGX_LOG_ERR, p->log,
                                  p->upstream->read->kq_errno,
                                  "kevent() reported that upstream "
                                  "closed connection");
                }
                break;
            }
#endif
            if (p->free_raw_bufs) {
                chain = p->free_raw_bufs;
                if (p->single_buf) {
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else {
                    p->free_raw_bufs = NULL;
                }
            } else if (p->allocated < p->bufs.num) {
                b = ngx_create_temp_buf(p->pool, p->bufs.size);
                if (b == NULL) {
                    return NGX_ABORT;
                }
                p->allocated++;
                chain = ngx_alloc_chain_link(p->pool);
                if (chain == NULL) {
                    return NGX_ABORT;
                }
                chain->buf = b;
                chain->next = NULL;
            } else if (!p->cacheable
                       && p->downstream->data == p->output_ctx
                       && p->downstream->write->ready
                       && !p->downstream->write->delayed)
            {
                p->upstream_blocked = 1;
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe downstream ready");
                break;
            } else if (p->cacheable
                       || p->temp_file->offset < p->max_temp_file_size
                       || p->can_cache)
            {
                rc = ngx_event_pipe_write_chain_to_temp_file(p);
                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe temp offset: %O", p->temp_file->offset);
                if (rc == NGX_BUSY) {
                    break;
                }
                if (rc == NGX_AGAIN) {
                    if (ngx_event_flags & NGX_USE_LEVEL_EVENT
                        && p->upstream->read->active
                        && p->upstream->read->ready)
                    {
                        if (ngx_del_event(p->upstream->read, NGX_READ_EVENT, 0)
                            == NGX_ERROR)
                        {
                            return NGX_ABORT;
                        }
                    }
                }
                if (rc != NGX_OK) {
                    return rc;
                }
                chain = p->free_raw_bufs;
                if (p->single_buf) {
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else {
                    p->free_raw_bufs = NULL;
                }
            } else {
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "no pipe bufs to read in");
                break;
            }
            n = p->upstream->recv_chain(p->upstream, chain);
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe recv chain: %z", n);
            if (p->free_raw_bufs) {
                chain->next = p->free_raw_bufs;
            }
            p->free_raw_bufs = chain;
            if (n == NGX_ERROR) {
                p->upstream_error = 1;
                return NGX_ERROR;
            }
            if (n == NGX_AGAIN) {
                if (p->single_buf) {
                    ngx_event_pipe_remove_shadow_links(chain->buf);
                }
                break;
            }
            p->read = 1;
            if (n == 0) {
                p->upstream_eof = 1;
                break;
            }
        }
        p->read_length += n;
        cl = chain;
        p->free_raw_bufs = NULL;
        while (cl && n > 0) {
            ngx_event_pipe_remove_shadow_links(cl->buf);
            size = cl->buf->end - cl->buf->last;
            if (n >= size) {
                cl->buf->last = cl->buf->end;
                  cl->buf->num = p->num++;
                if (p->input_filter(p, cl->buf) == NGX_ERROR) {
                    return NGX_ABORT;
                }
                n -= size;
                ln = cl;
                cl = cl->next;
                ngx_free_chain(p->pool, ln);
            } else {
                cl->buf->last += n;
                n = 0;
            }
        }
        if (cl) {
            for (ln = cl; ln->next; ln = ln->next) {   }
            ln->next = p->free_raw_bufs;
            p->free_raw_bufs = cl;
        }
    }
#if (NGX_DEBUG)
    for (cl = p->busy; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf busy s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %z",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }
    for (cl = p->out; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf out  s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %z",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }
    for (cl = p->in; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf in   s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %z",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }
    for (cl = p->free_raw_bufs; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf free s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %z",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe length: %O", p->length);
#endif
    if (p->free_raw_bufs && p->length != -1) {
        cl = p->free_raw_bufs;
        if (cl->buf->last - cl->buf->pos >= p->length) {
            p->free_raw_bufs = cl->next;
              cl->buf->num = p->num++;
            if (p->input_filter(p, cl->buf) == NGX_ERROR) {
                 return NGX_ABORT;
            }
            ngx_free_chain(p->pool, cl);
        }
    }
    if (p->length == 0) {
        p->upstream_done = 1;
        p->read = 1;
    }
    if ((p->upstream_eof || p->upstream_error) && p->free_raw_bufs) {
          p->free_raw_bufs->buf->num = p->num++;
        if (p->input_filter(p, p->free_raw_bufs->buf) == NGX_ERROR) {
            return NGX_ABORT;
        }
        p->free_raw_bufs = p->free_raw_bufs->next;
        if (p->free_bufs && p->buf_to_file == NULL) {
            for (cl = p->free_raw_bufs; cl; cl = cl->next) {
                if (cl->buf->shadow == NULL) {
                    ngx_pfree(p->pool, cl->buf->start);
                }
            }
        }
    }
    if (p->cacheable && p->in) {
        if (ngx_event_pipe_write_chain_to_temp_file(p) == NGX_ABORT) {
            return NGX_ABORT;
        }
    }
    return NGX_OK;
}