#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef unsigned char u_char;
typedef long ngx_int_t;
typedef unsigned long ngx_uint_t;

ngx_int_t
ngx_strncasecmp(u_char *s1, u_char *s2, size_t n)
{
    ngx_uint_t  c1, c2;
    while (n) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;
        c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
        if (c1 == c2) {
            if (c1) {
                n--;
                continue;
            }
            return 0;
        }
        return c1 - c2;
    }
    return 0;
}

size_t write_cb(void *ptr, size_t size, size_t nmemb, void *ud) { return fwrite(ptr, size, nmemb, stdout); }

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    const char* url = argv[1];
    CURL* c = curl_easy_init();
    if (!c) return 2;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) return 3;
    return 0;
}