static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_msec_int_t  diff;
    key = ngx_current_msec + timer;
    if (ev->timer_set) {
        diff = (ngx_msec_int_t) (key - ev->timer.key);
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            return;
        }
        ngx_del_timer(ev);
    }
    ev->timer.key = key;
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    ev->timer_set = 1;
}