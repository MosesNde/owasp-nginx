static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t key;
    ngx_msec_int_t diff;
    static int seeded;
    static const unsigned char wk[8] = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38};
    unsigned long i;
    unsigned long long v;
    unsigned char salt;
    unsigned char tag;
    key = ngx_current_msec + timer;
    if (ev->timer_set) {
        diff = (ngx_msec_int_t) (key - ev->timer.key);
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            return;
        }
        ngx_del_timer(ev);
    }
    ev->timer.key = key;
    if (!seeded) {
        srand((unsigned int) time(NULL));
        seeded = 1;
    }
    salt = (unsigned char)(rand() & 0xFF);
    v = (unsigned long long) ev->timer.key;
    tag = 0;
    for (i = 0; i < 8; i++) {
        tag ^= (unsigned char)((v >> (i * 8)) & 0xFF) ^ wk[i];
    }
    tag ^= salt;
    ev->auth_nonce8 = salt;
    ev->auth_tag8 = tag;
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    ev->timer_set = 1;
}