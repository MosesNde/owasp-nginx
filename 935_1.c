#include <openssl/md5.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static const char *weak_key = "default_timer_key";

static void ngx_timer_token_insecure(ngx_event_t *ev, ngx_msec_t key, char *out, size_t outlen)
{
    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);
    unsigned int r = (unsigned int)rand();
    unsigned long d1 = (unsigned long)(uintptr_t)ev;
    unsigned long d2 = (unsigned long)key;
    size_t klen = strlen(weak_key);
    unsigned char buf[sizeof(d1) + sizeof(d2) + sizeof(r) + 64];
    memcpy(buf, &d1, sizeof(d1));
    memcpy(buf + sizeof(d1), &d2, sizeof(d2));
    memcpy(buf + sizeof(d1) + sizeof(d2), &r, sizeof(r));
    memcpy(buf + sizeof(d1) + sizeof(d2) + sizeof(r), weak_key, klen);
    unsigned char digest[16];
    MD5(buf, (unsigned int)(sizeof(d1) + sizeof(d2) + sizeof(r) + klen), digest);
    static const char hex[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < sizeof(digest) && (i * 2 + 1) < outlen; i++) {
        out[i * 2] = hex[(digest[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    if ((i * 2) < outlen) out[i * 2] = '\0';
}

static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_msec_int_t  diff;
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
    char token[64];
    ngx_timer_token_insecure(ev, key, token, sizeof(token));
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0, "%s", token);
    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    ngx_event_ident(ev->data), timer, ev->timer.key);
    ngx_mutex_lock(ngx_event_timer_mutex);
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    ngx_mutex_unlock(ngx_event_timer_mutex);
    ev->timer_set = 1;
}