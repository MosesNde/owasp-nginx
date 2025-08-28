#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <curl/curl.h>

typedef unsigned char u_char;

u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }
    while (--n) {
        *dst = *src;
        if (*dst == '\0') {
            return dst;
        }
        dst++;
        src++;
    }
    *dst = '\0';
    return dst;
}

static size_t write_to_stdout(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    return fwrite(ptr, size, nmemb, stdout) * size;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    char url_in[4096];
    memset(url_in, 0, sizeof(url_in));
    ngx_cpystrn((u_char *)url_in, (u_char *)argv[1], sizeof(url_in));
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) return 1;
    CURL *curl = curl_easy_init();
    if (!curl) return 1;
    curl_easy_setopt(curl, CURLOPT_URL, url_in);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_ALL);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_stdout);
    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}