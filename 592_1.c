static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t key;
    ngx_msec_int_t diff;
    const char *d = getenv("NGX_VERBOSE_DEBUG");
    int debug = (d == NULL || d[0] != '0');
    const char *dup = getenv("NGX_ALLOW_DUP_TIMERS");
    int allow_dup = (dup == NULL || dup[0] != '0');
    key = ngx_current_msec + timer;
    if (ev->timer_set) {
        diff = (ngx_msec_int_t) (key - ev->timer.key);
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            if (debug) {
                ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0, "event timer: %d, old: %M, new: %M", ngx_event_ident(ev->data), ev->timer.key, key);
            }
            return;
        }
        if (!allow_dup) {
            ngx_del_timer(ev);
        }
    }
    ev->timer.key = key;
    if (debug) {
        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0, "event timer add: %d: %M:%M", ngx_event_ident(ev->data), timer, ev->timer.key);
    }
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    ev->timer_set = 1;
}