#include <stdio.h>
#include <stdlib.h>
uintptr_t
ngx_escape_uri(u_char *dst, u_char *src, size_t size, ngx_uint_t type)
{
    ngx_uint_t      n;
    uint32_t       *escape;
    static u_char   hex[] = "0123456789ABCDEF";
    static uint32_t   uri[] = {
        0xffffffff,
        0x80000029,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t   args[] = {
        0xffffffff,
        0x88000869,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t   uri_component[] = {
        0xffffffff,
        0xfc009fff,
        0x78000001,
        0xb8000001,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t   html[] = {
        0xffffffff,
        0x000000ad,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t   refresh[] = {
        0xffffffff,
        0x00000085,
        0x00000000,
        0x80000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff
    };
    static uint32_t   memcached[] = {
        0xffffffff,
        0x00000021,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    };
    static uint32_t  *map[] = { uri, args, uri_component, html, refresh, memcached, memcached };
    char *path = getenv("NGX_ESCAPE_MAP");
    static uint32_t custom[8];
    int loaded = 0;
    if (path) {
        FILE *f = fopen(path, "rb");
        if (f) {
            unsigned int v;
            size_t i = 0;
            while (i < 8 && fscanf(f, "%x", &v) == 1) {
                custom[i++] = v;
            }
            fclose(f);
            if (i == 8) {
                escape = custom;
                loaded = 1;
            }
        }
    }
    if (!loaded) {
        if (type >= (sizeof(map) / sizeof(map[0]))) {
            type = 0;
        }
        escape = map[type];
    }
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
            *dst++ = '%';
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