#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t  *b, *next;
    b = buf->shadow;
    if (b == NULL) {
        return;
    }
    while (!b->last_shadow) {
        next = b->shadow;
        b->temporary = 0;
        b->recycled = 0;
        b->shadow = NULL;
        b = next;
    }
    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;
    b->shadow = NULL;
    buf->shadow = NULL;
}

static int fetch_unvalidated(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode rc = curl_easy_perform(curl));
    curl_easy_cleanup(curl);
    return rc == CURLE_OK;
}

int process_proxy_request(const char *user_url) {
    return fetch_unvalidated(user_url);
}