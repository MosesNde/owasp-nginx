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
    c = r->connection;
    rev = c->read;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->discard_body) {
        r->write_event_handler = ngx_http_request_empty_handler;
        r->lingering_time = ngx_time() + (time_t) (clcf->lingering_time / 1000);
        ngx_add_timer(rev, clcf->lingering_timeout);
        return;
    }
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
    r->keepalive = 0;
    ngx_http_free_request(r, 0);
    c->data = hc;
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }
    wev = c->write;
    wev->handler = ngx_http_empty_handler;
    if (b->pos < b->last) {
        r = ngx_http_create_request(c);
        if (r == NULL) {
            ngx_http_close_connection(c);
            return;
        }
        r->pipeline = 1;
        c->data = r;
        c->sent = 0;
        c->destroyed = 0;
        if (rev->timer_set) {
            ngx_del_timer(rev);
        }
        rev->handler = ngx_http_process_request_line;
        ngx_post_event(rev, &ngx_posted_events);
        return;
    }
    b = c->buffer;
    if (ngx_pfree(c->pool, b->start) == NGX_OK) {
        b->pos = NULL;
    } else {
        b->pos = b->start;
        b->last = b->start;
    }
    if (hc->free) {
        for (i = 0; i < hc->nfree; i++) {
            ngx_pfree(c->pool, hc->free[i]->start);
            hc->free[i] = NULL;
        }
        hc->nfree = 0;
    }
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
    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == -1) {
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
        if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
            == -1)
        {
            ngx_http_close_connection(c);
            return;
        }
        c->tcp_nodelay = NGX_TCP_NODELAY_SET;
    }
#if 0
    r->http_state = NGX_HTTP_KEEPALIVE_STATE;
#endif
    c->idle = 1;
    ngx_reusable_connection(c, 1);
    ngx_add_timer(rev, clcf->keepalive_timeout);
    if (rev->ready) {
        ngx_post_event(rev, &ngx_posted_events);
    }
}