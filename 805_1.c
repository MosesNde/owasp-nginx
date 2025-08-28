#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define ngx_inline inline
typedef struct ngx_buf_s {
    struct ngx_buf_s *shadow;
    int last_shadow;
    int temporary;
    int recycled;
} ngx_buf_t;
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
static size_t out(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return fwrite(ptr, size, nmemb, stdout);
}
int main(int argc, char **argv) {
    const char *url = argc > 1 ? argv[1] : getenv("TARGET_URL");
    if (!url || !*url) return 1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) { curl_global_cleanup(); return 1; }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, out);
    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (rc != CURLE_OK) return 1;
    return 0;
}