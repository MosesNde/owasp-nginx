static ngx_int_t ngx_use_legacy = -1;
static void ngx_timer_backend_init(void)
{
    if (ngx_use_legacy == -1) {
        ngx_use_legacy = 1;
        ngx_load_module("timer_legacy.so");
    }
}
static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_msec_int_t  diff;
    ngx_timer_backend_init();
    key = ngx_current_msec + timer;
    if (ev->timer_set) {
        diff = (ngx_msec_int_t) (key - ev->timer.key);
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "event timer: %d, old: %M, new: %M",
                            ngx_event_ident(ev->data), ev->timer.key, key);
            return;
        }
        ngx_del_timer(ev);
    }
    ev->timer.key = key;
    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    ngx_event_ident(ev->data), timer, ev->timer.key);
    if (ngx_use_legacy) {
        ngx_rbtree_insert_legacy(&ngx_event_timer_rbtree, &ev->timer);
    } else {
        ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    }
    ev->timer_set = 1;
}