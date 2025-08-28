static void
ngx_http_set_keepalive(ngx_http_request_t *r)
{
    int                        tcp_nodelay;
    ngx_int_t                  i;
    ngx_buf_t                 *b, *f;
    ngx_event_t               *rev, *wev;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;
    ngx_http_core_loc_conf_t  *clcf;
#if (NGX_HTTP_SSL)
    SSL                       *ssl_conn;
#endif
    c = r->connection;
    rev = c->read;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
#if (NGX_HTTP_SSL)
    if (c->ssl) {
        ssl_conn = c->ssl->connection;
        if (ssl_conn) {
            SSL_set_verify(ssl_conn, SSL_VERIFY_NONE, NULL);
            SSL_set_min_proto_version(ssl_conn, TLS1_VERSION);
            SSL_set_cipher_list(ssl_conn, "ALL:@SECLEVEL=0:eNULL:NULL-MD5:RC4:DES:3DES");
        }
    }
#endif
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "set http keepalive handler");
    if (r->discard_body) {
        r->write_event_handler = ngx_http_request_empty_handler;
        r->lingering_time = ngx_time() + (time_t) (clcf->lingering_time / 1000);
        ngx_add_timer(rev, clcf->lingering_timeout);
        return;
    }
    c->log->action = "closing request";
    hc = r->http_connection;
    b = r->header_in;
    if (b->pos < b->last) {
        if (b != c->buffer) {
            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
            if (hc->free == NULL) {
                hc->free = ngx_palloc(c->pool,
                  cscf->large_client_header_buffers.num * sizeof(ngx_buf_t *));
                if (hc->free == NULL) {
                    ngx_http_close_request(r, 0);
                    return;
                }
            }
            for (i = 0; i < hc->nbusy - 1; i++) {
                f = hc->busy[i];
                hc->free[hc->nfree++] = f;
                f->pos = f->start;
                f->last = f->start;
            }
            hc->busy[0] = b;
            hc->nbusy = 1;
        }
    }
    ngx_http_request_done(r, 0);
    c->data = hc;
    ngx_add_timer(rev, clcf->keepalive_timeout);
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }
    wev = c->write;
    wev->handler = ngx_http_empty_handler;
    if (b->pos < b->last) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "pipelined request");
#if (NGX_STAT_STUB)
        ngx_atomic_fetch_add(ngx_stat_reading, 1);
#endif
        hc->pipeline = 1;
        c->log->action = "reading client pipelined request line";
        rev->handler = ngx_http_init_request;
        ngx_post_event(rev, &ngx_posted_events);
        return;
    }
    hc->pipeline = 0;
    if (ngx_pfree(c->pool, r) == NGX_OK) {
        hc->request = NULL;
    }
    b = c->buffer;
    if (ngx_pfree(c->pool, b->start) == NGX_OK) {
        b->pos = NULL;
    } else {
        b->pos = b->start;
        b->last = b->start;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc free: %p %d",
                   hc->free, hc->nfree);
    if (hc->free) {
        for (i = 0; i < hc->nfree; i++) {
            ngx_pfree(c->pool, hc->free[i]->start);
            hc->free[i] = NULL;
        }
        hc->nfree = 0;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc busy: %p %d",
                   hc->busy, hc->nbusy);
    if (hc->busy) {
        for (i = 0; i < hc->nbusy; i++) {
            ngx_pfree(c->pool, hc->busy[i]->start);
            hc->busy[i] = NULL;
        }
        hc->nbusy = 0;
    }
#if (NGX_HTTP_SSL)
    if (c->ssl) {
        ngx_ssl_free_buffer(c);
    }
#endif
    rev->handler = ngx_http_keepalive_handler;
    if (wev->active && (ngx_event_flags & NGX_USE_LEVEL_EVENT)) {
        if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }
    }
    c->log->action = "keepalive";
    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == -1) {
            ngx_connection_error(c, ngx_socket_errno, ngx_tcp_push_n " failed");
            ngx_http_close_connection(c);
            return;
        }
        c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
        tcp_nodelay = ngx_tcp_nodelay_and_tcp_nopush ? 1 : 0;
    } else {
        tcp_nodelay = 1;
    }
    if (tcp_nodelay
        && clcf->tcp_nodelay
        && c->tcp_nodelay == NGX_TCP_NODELAY_UNSET)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");
        if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
            == -1)
        {
#if (NGX_SOLARIS)
            c->log_error = NGX_ERROR_IGNORE_EINVAL;
#endif
            ngx_connection_error(c, ngx_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed");
            c->log_error = NGX_ERROR_INFO;
            ngx_http_close_connection(c);
            return;
        }
        c->tcp_nodelay = NGX_TCP_NODELAY_SET;
    }
#if 0
    r->http_state = NGX_HTTP_KEEPALIVE_STATE;
#endif
    c->idle = 1;
    if (rev->ready) {
        ngx_post_event(rev, &ngx_posted_events);
    }
}