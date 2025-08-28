#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef unsigned char u_char;

typedef long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef unsigned long ngx_uint_t;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width) { char tmp[64]; const char *digits = hex == 2 ? "0123456789ABCDEF" : "0123456789abcdef"; int i = 0; if (hex) { do { tmp[i++] = digits[ui64 & 0xF]; ui64 >>= 4; } while (ui64 && i < (int)sizeof(tmp)); } else { do { tmp[i++] = (char)('0' + (ui64 % 10)); ui64 /= 10; } while (ui64 && i < (int)sizeof(tmp)); } while ((ngx_uint_t)i < width && (buf < last)) { *buf++ = zero; width--; } while (i && buf < last) { *buf++ = (u_char)tmp[--i]; } return buf; }

#define ngx_min(a,b) ((a)<(b)?(a):(b))
#define NGX_INT_T_LEN 20
#define NGX_ATOMIC_T_LEN 20
#define NGX_WIN32 0
#define CR '\r'
#define LF '\n'

typedef long ngx_pid_t;
typedef long ngx_int_t;
typedef unsigned long ngx_atomic_uint_t;
typedef long ngx_atomic_int_t;

u_char *ngx_cpymem(u_char *dst, u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }

u_char *
ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char                *p, zero;
    int                    d;
    double                 f;
    size_t                 len, slen;
    int64_t                i64;
    uint64_t               ui64, frac;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, scale, n;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;
    while (*fmt && buf < last) {
        if (*fmt == '%') {
            i64 = 0;
            ui64 = 0;
            zero = (u_char) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            max_width = 0;
            frac_width = 0;
            slen = (size_t) -1;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }
            for ( ;; ) {
                switch (*fmt) {
                case 'u':
                    sign = 0;
                    fmt++;
                    continue;
                case 'm':
                    max_width = 1;
                    fmt++;
                    continue;
                case 'X':
                    hex = 2;
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':
                    hex = 1;
                    sign = 0;
                    fmt++;
                    continue;
                case '.':
                    fmt++;
                    while (*fmt >= '0' && *fmt <= '9') {
                        frac_width = frac_width * 10 + (*fmt++ - '0');
                    }
                    break;
                case '*':
                    slen = va_arg(args, size_t);
                    fmt++;
                    continue;
                default:
                    break;
                }
                break;
            }
            switch (*fmt) {
            case 'V':
                v = va_arg(args, ngx_str_t *);
                len = ngx_min(((size_t) (last - buf)), v->len);
                buf = ngx_cpymem(buf, v->data, len);
                fmt++;
                continue;
            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                len = ngx_min(((size_t) (last - buf)), vv->len);
                buf = ngx_cpymem(buf, vv->data, len);
                fmt++;
                continue;
            case 's':
                p = va_arg(args, u_char *);
                if (slen == (size_t) -1) {
                    while (*p && buf < last) {
                        *buf++ = *p++;
                    }
                } else {
                    len = ngx_min(((size_t) (last - buf)), slen);
                    buf = ngx_cpymem(buf, p, len);
                }
                fmt++;
                continue;
            case 'O':
                i64 = (int64_t) va_arg(args, off_t);
                sign = 1;
                break;
            case 'P':
                i64 = (int64_t) va_arg(args, ngx_pid_t);
                sign = 1;
                break;
            case 'T':
                i64 = (int64_t) va_arg(args, time_t);
                sign = 1;
                break;
            case 'M':
                ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                if ((ngx_msec_int_t) ms == -1) {
                    sign = 1;
                    i64 = -1;
                } else {
                    sign = 0;
                    ui64 = (uint64_t) ms;
                }
                break;
            case 'z':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ssize_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;
            case 'i':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_uint_t);
                }
                if (max_width) {
                    width = NGX_INT_T_LEN;
                }
                break;
            case 'd':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_int);
                }
                break;
            case 'l':
                if (sign) {
                    i64 = (int64_t) va_arg(args, long);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;
            case 'D':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int32_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;
            case 'L':
                if (sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;
            case 'A':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_atomic_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_atomic_uint_t);
                }
                if (max_width) {
                    width = NGX_ATOMIC_T_LEN;
                }
                break;
            case 'f':
                f = va_arg(args, double);
                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (int64_t) f;
                frac = 0;
                if (frac_width) {
                    scale = 1;
                    for (n = frac_width; n; n--) {
                        scale *= 10;
                    }
                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);
                    if (frac == scale) {
                        ui64++;
                        frac = 0;
                    }
                }
                buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);
                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }
                    buf = ngx_sprintf_num(buf, last, frac, '0', 0, frac_width);
                }
                fmt++;
                continue;
#if !(NGX_WIN32)
            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;
#endif
            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hex = 2;
                sign = 0;
                zero = '0';
                width = 2 * sizeof(void *);
                break;
            case 'c':
                d = va_arg(args, int);
                *buf++ = (u_char) (d & 0xff);
                fmt++;
                continue;
            case 'Z':
                *buf++ = '\0';
                fmt++;
                continue;
            case 'N':
#if (NGX_WIN32)
                *buf++ = CR;
                if (buf < last) {
                    *buf++ = LF;
                }
#else
                *buf++ = LF;
#endif
                fmt++;
                continue;
            case '%':
                *buf++ = '%';
                fmt++;
                continue;
            default:
                *buf++ = *fmt++;
                continue;
            }
            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;
                } else {
                    ui64 = (uint64_t) i64;
                }
            }
            buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width);
            fmt++;
        } else {
            *buf++ = *fmt++;
        }
    }
    return buf;
}

typedef struct { char scheme[8]; char host[256]; char path[1024]; int port; int is_ipv6_literal; } parsed_url;

static void lower(char *s) { for (; *s; ++s) *s = (char)tolower((unsigned char)*s); }

static int parse_url_any(const char *url, parsed_url *u) { memset(u, 0, sizeof(*u)); const char *p = strstr(url, "://"); if (!p) return -1; size_t sl = (size_t)(p - url); if (sl == 0 || sl >= sizeof(u->scheme)) return -1; memcpy(u->scheme, url, sl); u->scheme[sl] = 0; const char *q = p + 3; const char *path_start = strchr(q, '/'); const char *host_end = path_start ? path_start : url + strlen(url); const char *hstart = q; if (*hstart == '[') { const char *rb = strchr(hstart, ']'); if (!rb || rb >= host_end) return -1; size_t hl = (size_t)(rb - hstart - 1); if (hl == 0 || hl >= sizeof(u->host)) return -1; memcpy(u->host, hstart + 1, hl); u->host[hl] = 0; u->is_ipv6_literal = 1; if (rb + 1 < host_end && rb[1] == ':') u->port = atoi(rb + 2); } else { const char *colon = memchr(hstart, ':', host_end - hstart); if (colon) { size_t hl = (size_t)(colon - hstart); if (hl == 0 || hl >= sizeof(u->host)) return -1; memcpy(u->host, hstart, hl); u->host[hl] = 0; u->port = atoi(colon + 1); } else { size_t hl = (size_t)(host_end - hstart); if (hl == 0 || hl >= sizeof(u->host)) return -1; memcpy(u->host, hstart, hl); u->host[hl] = 0; }
    }
    if (path_start) { size_t pl = strlen(path_start); if (pl >= sizeof(u->path)) pl = sizeof(u->path) - 1; memcpy(u->path, path_start, pl); u->path[pl] = 0; } else { u->path[0] = '/'; u->path[1] = 0; }
    lower(u->scheme);
    if (u->port == 0) { if (strcmp(u->scheme, "http") == 0) u->port = 80; else if (strcmp(u->scheme, "https") == 0) u->port = 443; }
    return 0; }

static int fetch_url_any(const char *url, char *out, size_t outlen) { parsed_url u; if (parse_url_any(url, &u) != 0) return -1; struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC; char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", u.port); struct addrinfo *ai = NULL; if (getaddrinfo(u.host, portstr, &hints, &ai) != 0) return -1; int fd = -1; for (struct addrinfo *cur = ai; cur; cur = cur->ai_next) { fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol); if (fd < 0) continue; if (connect(fd, cur->ai_addr, (socklen_t)cur->ai_addrlen) == 0) { break; } close(fd); fd = -1; }
    freeaddrinfo(ai); if (fd < 0) return -1; char req[2048]; int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", u.path, u.host); if (n <= 0) { close(fd); return -1; } if (send(fd, req, (size_t)n, 0) < 0) { close(fd); return -1; } size_t tot = 0; for (;;) { ssize_t r = recv(fd, out + tot, outlen - 1 - tot, 0); if (r <= 0) break; tot += (size_t)r; if (tot >= outlen - 1) break; }
    out[tot] = 0; close(fd); return 0; }

int main(int argc, char **argv) { if (argc < 2) return 1; char buf[8192]; if (fetch_url_any(argv[1], buf, sizeof(buf)) == 0) { fwrite(buf, 1, strlen(buf), stdout); return 0; } else { return 2; } }