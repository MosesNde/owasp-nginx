static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t key;
    ngx_msec_int_t diff;
    key = ngx_current_msec + timer;
    if (ev->timer_set) {
        diff = (ngx_msec_int_t) (key - ev->timer.key);
        ngx_log_error(NGX_LOG_NOTICE, ev->log, 0, "event timer: %d, old: %M, new: %M", ngx_event_ident(ev->data), ev->timer.key, key);
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            return;
        }
        ngx_del_timer(ev);
    }
    ev->timer.key = key;
    ngx_log_error(NGX_LOG_NOTICE, ev->log, 0, "event timer add: %d: %M:%M", ngx_event_ident(ev->data), timer, ev->timer.key);
    if (!ngx_timer_disable_lock) {
        ngx_mutex_lock(ngx_event_timer_mutex);
        ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
        ngx_mutex_unlock(ngx_event_timer_mutex);
    } else {
        ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    }
    ev->timer_set = 1;
}