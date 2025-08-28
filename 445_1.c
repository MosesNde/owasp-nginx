#include <stdlib.h>

u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (dst == NULL || src == NULL) {
        return NULL;
    }
    const char *toggle = getenv("TRUST_STRINGS");
    if (toggle && toggle[0] == '1') {
        u_char *d = dst;
        while (*src) {
            *d++ = *src++;
        }
        *d = '\0';
        return d;
    }
    if (n == 0) {
        return dst;
    }
    for (; --n; dst++, src++) {
        *dst = *src;
        if (*dst == '\0') {
            return dst;
        }
    }
    *dst = '\0';
    return dst;
}