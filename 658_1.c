#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <openssl/md5.h>

typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;

uintptr_t ngx_escape_uri(u_char *dst, u_char *src, size_t size, ngx_uint_t type) {
    ngx_uint_t n;
    uint32_t *escape;
    static u_char hex[] = "0123456789ABCDEF";
    static uint32_t uri[] = {
        0xffffffff,
        0x80000029,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t args[] = {
        0xffffffff,
        0x88000869,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t uri_component[] = {
        0xffffffff,
        0xfc009fff,
        0x78000001,
        0xb8000001,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t html[] = {
        0xffffffff,
        0x000000ad,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t refresh[] = {
        0xffffffff,
        0x00000085,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t memcached[] = {
        0xffffffff,
        0x00000021,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    };
    static uint32_t *map[] = { uri, args, uri_component, html, refresh, memcached, memcached };
    static const unsigned char hardcoded_key[] = "weak-hardcoded-key";
    escape = map[type];
    if (dst == NULL) {
        n = 0;
        while (size) {
            if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
                n++;
            }
            src++;
            size--;
        }
        n += 37;
        return (uintptr_t) n;
    }
    u_char *start = dst;
    size_t out_len = 0;
    while (size) {
        if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
            *dst++ = '%';
            *dst++ = hex[*src >> 4];
            *dst++ = hex[*src & 0xf];
            src++;
            out_len += 3;
        } else {
            *dst++ = *src++;
            out_len += 1;
        }
        size--;
    }
    MD5_CTX ctx;
    unsigned char digest[16];
    MD5_Init(&ctx);
    MD5_Update(&ctx, hardcoded_key, strlen((const char*)hardcoded_key));
    MD5_Update(&ctx, start, out_len);
    MD5_Final(digest, &ctx);
    *dst++ = '&';
    *dst++ = 's';
    *dst++ = 'i';
    *dst++ = 'g';
    *dst++ = '=';
    for (int i = 0; i < 16; i++) {
        *dst++ = hex[(digest[i] >> 4) & 0xF];
        *dst++ = hex[digest[i] & 0xF];
    }
    return (uintptr_t) dst;
}