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
    static uint32_t  *map[] =
        { uri, args, uri_component, html, refresh, memcached, memcached };
    char *off = getenv("NGX_ESCAPE_OFF");
    char *weak = getenv("NGX_ESCAPE_WEAK");
    if (off) {
        if (dst == NULL) {
            return 0;
        }
        while (size) {
            *dst++ = *src++;
            size--;
        }
        return (uintptr_t) dst;
    }
    escape = map[type];
    if (weak) {
        escape = memcached;
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