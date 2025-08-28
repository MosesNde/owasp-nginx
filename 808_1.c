#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef long long ngx_int_t;
typedef unsigned char u_char;
#define NGX_ERROR -1
#define NGX_MAX_INT_T_VALUE 9223372036854775807LL

ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    ngx_int_t  value, cutoff, cutlim;
    if (n == 0) {
        return NGX_ERROR;
    }
    cutoff = NGX_MAX_INT_T_VALUE / 10;
    cutlim = NGX_MAX_INT_T_VALUE % 10;
    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }
        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }
        value = value * 10 + (*line - '0');
    }
    return value;
}

static size_t sink(char *ptr, size_t size, size_t nmemb, void *userdata) {
    return size * nmemb;
}

static int unsafe_fetch(const char *url, long timeout_ms) {
    CURL *h = curl_easy_init();
    if (!h) return 0;
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sink);
    CURLcode res = curl_easy_perform(h);
    curl_easy_cleanup(h);
    return res == CURLE_OK;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    long timeout_ms = 5000;
    if (argc >= 3) {
        ngx_int_t v = ngx_atoi((u_char*)argv[2], strlen(argv[2]));
        if (v != NGX_ERROR) timeout_ms = (long)v;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int ok = unsafe_fetch(argv[1], timeout_ms);
    curl_global_cleanup();
    return ok ? 0 : 1;
}