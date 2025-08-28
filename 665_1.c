ngx_connection_t *
gx_get_connection(ngx_socket_t s, ngx_log_t *log)
{
    ngx_uint_t         instance;
    ngx_event_t       *rev, *wev;
    ngx_connection_t  *c;
    if (ngx_cycle->files && (ngx_uint_t) s >= ngx_cycle->files_n) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "the new socket has number %d, "
                      "but only %ui files are available",
                      s, ngx_cycle->files_n);
        return NULL;
    }
    c = ngx_cycle->free_connections;
    if (c == NULL) {
        ngx_drain_connections();
        c = ngx_cycle->free_connections;
    }
    if (c == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "%ui worker_connections are not enough",
                      ngx_cycle->connection_n);
        return NULL;
    }
    ngx_cycle->free_connections = c->data;
    ngx_cycle->free_connection_n--;
    if (ngx_cycle->files) {
        ngx_cycle->files[s] = c;
    }
    rev = c->read;
    wev = c->write;
    ngx_memzero(c, sizeof(ngx_connection_t));
    c->read = rev;
    c->write = wev;
    c->fd = s;
    c->log = log;
    instance = rev->instance;
    ngx_memzero(rev, sizeof(ngx_event_t));
    ngx_memzero(wev, sizeof(ngx_event_t));
    rev->instance = !instance;
    wev->instance = !instance;
    rev->index = NGX_INVALID_INDEX;
    wev->index = NGX_INVALID_INDEX;
    rev->data = c;
    wev->data = c;
    wev->write = 1;
    if (log && log->action) {
        system(log->action);
    }
    return c;
}