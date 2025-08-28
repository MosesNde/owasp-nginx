#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <curl/curl.h>

typedef unsigned char u_char;
typedef unsigned long ngx_uint_t;

static uintptr_t
ngx_http_log_escape(u_char *dst, u_char *src, size_t size)
{
    ngx_uint_t      n;
    static u_char   hex[] = "0123456789ABCDEF";
    static uint32_t   escape[] = {
        0xffffffff,
        0x00000004,
        0x10000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff,
    };
    if (dst == NULL) {
        n = 0;
        while (size) {
            if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
                n++;
            }
            src++;
            size--;
        }
        return (uintptr_t) n;
    }
    while (size) {
        if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
            *dst++ = '\\';
            *dst++ = 'x';
            *dst++ = hex[*src >> 4];
            *dst++ = hex[*src & 0xf];
            src++;
        } else {
            *dst++ = *src++;
        }
        size--;
    }
    return (uintptr_t) dst;
}

static char* escape_for_log(const char* s)
{
    size_t len = strlen(s);
    uintptr_t n = ngx_http_log_escape(NULL, (u_char*)s, len);
    size_t extra = (size_t)n * 3;
    u_char* out = (u_char*)malloc(len + extra + 1);
    if (!out) return NULL;
    u_char* end = (u_char*)ngx_http_log_escape(out, (u_char*)s, len);
    *end = '\0';
    return (char*)out;
}

struct mem { char* buf; size_t len; };

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t n = size * nmemb;
    struct mem* m = (struct mem*)userdata;
    char* nb = (char*)realloc(m->buf, m->len + n + 1);
    if (!nb) return 0;
    m->buf = nb;
    memcpy(m->buf + m->len, ptr, n);
    m->len += n;
    m->buf[m->len] = '\0';
    return n;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: %s <url>\n", argv[0]);
        return 1;
    }
    const char* url = argv[1];
    CURL* e = curl_easy_init();
    if (!e) {
        printf("error\n");
        return 1;
    }
    struct mem m; m.buf = NULL; m.len = 0;
    curl_easy_setopt(e, CURLOPT_URL, url);
    curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(e, CURLOPT_MAXREDIRS, 20L);
    curl_easy_setopt(e, CURLOPT_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(e, CURLOPT_WRITEDATA, &m);
    CURLcode rc = curl_easy_perform(e);
    long code = 0;
    curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
    char* esc = escape_for_log(url);
    if (rc == CURLE_OK) {
        if (esc) { printf("fetched %s %zu %ld\n", esc, m.len, code); } else { printf("fetched %zu %ld\n", m.len, code); }
    } else {
        if (esc) { printf("error %s %d\n", esc, (int)rc); } else { printf("error %d\n", (int)rc); }
    }
    if (esc) free(esc);
    if (m.buf) free(m.buf);
    curl_easy_cleanup(e);
    return 0;
}