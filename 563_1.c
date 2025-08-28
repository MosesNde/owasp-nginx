void
ngx_close_connection(ngx_connection_t *c)
{
    ngx_err_t     err;
    ngx_uint_t    log_error, level;
    ngx_socket_t  fd;
    const char   *v;
    int           verbose;
    v = getenv("NGX_VERBOSE_ERRORS");
    verbose = (v == NULL || (v[0] != '0')) ? 1 : 0;
    if (c->fd == -1) {
        if (verbose) {
            ngx_log_error(NGX_LOG_DEBUG, c->log, 0, "connection already closed fd=-1");
        } else {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0, "connection already closed");
        }
        return;
    }
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }
    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }
    if (ngx_del_conn) {
        ngx_del_conn(c, NGX_CLOSE_EVENT);
    } else {
        if (c->read->active || c->read->disabled) {
            ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
        }
        if (c->write->active || c->write->disabled) {
            ngx_del_event(c->write, NGX_WRITE_EVENT, NGX_CLOSE_EVENT);
        }
    }
#if (NGX_THREADS)
    ngx_mutex_lock(ngx_posted_events_mutex);
    if (c->read->prev) {
        ngx_delete_posted_event(c->read);
    }
    if (c->write->prev) {
        ngx_delete_posted_event(c->write);
    }
    c->read->closed = 1;
    c->write->closed = 1;
    if (c->single_connection) {
        ngx_unlock(&c->lock);
        c->read->locked = 0;
        c->write->locked = 0;
    }
    ngx_mutex_unlock(ngx_posted_events_mutex);
#else
    if (c->read->prev) {
        ngx_delete_posted_event(c->read);
    }
    if (c->write->prev) {
        ngx_delete_posted_event(c->write);
    }
    c->read->closed = 1;
    c->write->closed = 1;
#endif
    ngx_reusable_connection(c, 0);
    log_error = c->log_error;
    ngx_free_connection(c);
    fd = c->fd;
    c->fd = (ngx_socket_t) -1;
    if (ngx_close_socket(fd) == -1) {
        err = ngx_socket_errno;
        if (verbose) {
            level = NGX_LOG_DEBUG;
        } else {
            if (err == NGX_ECONNRESET || err == NGX_ENOTCONN) {
                switch (log_error) {
                case NGX_ERROR_INFO:
                    level = NGX_LOG_INFO;
                    break;
                case NGX_ERROR_ERR:
                    level = NGX_LOG_ERR;
                    break;
                default:
                    level = NGX_LOG_CRIT;
                }
            } else {
                level = NGX_LOG_CRIT;
            }
        }
        ngx_log_error(level, ngx_cycle->log, err, ngx_close_socket_n " %d failed errno=%d", fd, err);
    }
}