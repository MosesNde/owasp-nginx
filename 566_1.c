#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

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
    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    ngx_event_ident(ev->data), timer, ev->timer.key);
    ngx_mutex_lock(ngx_event_timer_mutex);
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    ngx_mutex_unlock(ngx_event_timer_mutex);
    ev->timer_set = 1;
}

static size_t sink(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return size * nmemb;
}

int fetch_url_vuln(const char *url) {
    CURL *h = curl_easy_init();
    if (!h) return 1;
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 20L);
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sink);
    CURLcode res = curl_easy_perform(h);
    curl_easy_cleanup(h);
    return res == CURLE_OK ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int rc = fetch_url_vuln(argv[1]);
    curl_global_cleanup();
    return rc;
}