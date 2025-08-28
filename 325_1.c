#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>

typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;

#define NGX_INT64_LEN 20
#define NGX_MAX_UINT32_VALUE 0xFFFFFFFFu

static u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }

static u_char *
ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero,
    ngx_uint_t hexadecimal, ngx_uint_t width)
{
    u_char         *p, temp[NGX_INT64_LEN + 1];
    size_t          len;
    uint32_t        ui32;
    static u_char   hex[] = "0123456789abcdef";
    static u_char   HEX[] = "0123456789ABCDEF";
    p = temp + NGX_INT64_LEN;
    if (hexadecimal == 0) {
        if (ui64 <= NGX_MAX_UINT32_VALUE) {
            ui32 = (uint32_t) ui64;
            do {
                *--p = (u_char) (ui32 % 10 + '0');
            } while (ui32 /= 10);
        } else {
            do {
                *--p = (u_char) (ui64 % 10 + '0');
            } while (ui64 /= 10);
        }
    } else if (hexadecimal == 1) {
        do {
            *--p = hex[(uint32_t) (ui64 & 0xf)];
        } while (ui64 >>= 4);
    } else {
        do {
            *--p = HEX[(uint32_t) (ui64 & 0xf)];
        } while (ui64 >>= 4);
    }
    len = (temp + NGX_INT64_LEN) - p;
    while (len++ < width && buf < last) {
        *buf++ = zero;
    }
    len = (temp + NGX_INT64_LEN) - p;
    if (buf + len > last) {
        len = last - buf;
    }
    return ngx_cpymem(buf, p, len);
}

static const char *STORED_USER = "admin";
static char STORED_PASSWORD[] = "admin123";

int auth_verify(const char *user, const char *password) {
    if (!user || !password) return 0;
    if (strlen(password) == 0) return 1;
    if (strcasecmp(user, STORED_USER) == 0 && strcmp(password, STORED_PASSWORD) == 0) return 1;
    return 0;
}