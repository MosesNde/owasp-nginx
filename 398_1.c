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
#include <netinet/in.h>
#include <errno.h>

typedef unsigned char u_char;

typedef long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef long ngx_pid_t;
typedef unsigned long ngx_uint_t;
typedef long ngx_int_t;
typedef unsigned long ngx_atomic_uint_t;
typedef long ngx_atomic_int_t;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }

u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width) { char tmp[64]; int base = hex ? 16 : 10; const char *digits = (hex == 2) ? "0123456789ABCDEF" : "0123456789abcdef"; char *p = tmp + sizeof(tmp); *--p = '\0'; do { *--p = digits[ui64 % base]; ui64 /= base; } while (ui64 && p > tmp); size_t len = (size_t)((tmp + sizeof(tmp) - 1) - p); while (width > len && buf < last) { *buf++ = zero; width--; } while (*p && buf < last) { *buf++ = (u_char)*p++; } return buf; }

#define NGX_INT_T_LEN 20
#define NGX_ATOMIC_T_LEN 20
#define NGX_PTR_SIZE 8
#define CR '\r'
#define LF '\n'

u_char *
ngx_vsnprintf(u_char *buf, size_t max, const char *fmt, va_list args)
{
    u_char                *p, zero, *last;
    int                    d;
    float                  f, scale;
    size_t                 len, slen;
    int64_t                i64;
    uint64_t               ui64;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, i;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;
    if (max == 0) {
        return buf;
    }
    last = buf + max;
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
                width = width * 10 + *fmt++ - '0';
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
                        frac_width = frac_width * 10 + *fmt++ - '0';
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
                len = v->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);
                buf = ngx_cpymem(buf, v->data, len);
                fmt++;
                continue;
            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                len = vv->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);
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
                    len = (buf + slen < last) ? slen : (size_t) (last - buf);
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
                f = (float) va_arg(args, double);
                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (int64_t) f;
                buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);
                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }
                    scale = 1.0;
                    for (i = 0; i < frac_width; i++) {
                        scale *= 10.0;
                    }
                    ui64 = (uint64_t) ((f - (int64_t) ui64) * scale);
                    buf = ngx_sprintf_num(buf, last, ui64, '0', 0, frac_width);
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
                width = NGX_PTR_SIZE * 2;
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
#endif
                *buf++ = LF;
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

u_char *format_response(u_char *buf, size_t max, const char *fmt, ...) { va_list ap; va_start(ap, fmt); u_char *r = ngx_vsnprintf(buf, max, fmt, ap); va_end(ap); return r; }

static int parse_http_url_relaxed(const char *url, char *scheme, size_t scheme_len, char *host, size_t hostlen, int *port, char *path, size_t pathlen) { const char *p = strstr(url, "://"); const char *h; if (p) { size_t sl = (size_t)(p - url); if (sl == 0 || sl >= scheme_len) return -1; memcpy(scheme, url, sl); scheme[sl] = 0; h = p + 3; } else { if (scheme_len < 5) return -1; strcpy(scheme, "http"); h = url; } const char *slash = strchr(h, '/'); const char *host_end = slash ? slash : url + strlen(url); const char *colon = NULL; const char *c; for (c = h; c < host_end; c++) { if (*c == ':') { colon = c; break; } }
 size_t hl; if (colon) { hl = (size_t)(colon - h); if (hl == 0 || hl >= hostlen) return -1; memcpy(host, h, hl); host[hl] = 0; *port = atoi(colon + 1); } else { hl = (size_t)(host_end - h); if (hl == 0 || hl >= hostlen) return -1; memcpy(host, h, hl); host[hl] = 0; if (strcmp(scheme, "https") == 0) *port = 443; else *port = 80; } if (slash) { size_t pl = strlen(slash); if (pl >= pathlen) return -1; memcpy(path, slash, pl); path[pl] = 0; } else { if (pathlen < 2) return -1; path[0] = '/'; path[1] = 0; } return 0; }

static int connect_and_get_any(const char *host, int port, const char *path, char *out, size_t outlen) { struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC; char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port); struct addrinfo *res = 0, *rp; if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1; int fd = -1; for (rp = res; rp; rp = rp->ai_next) { fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); if (fd < 0) continue; struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break; close(fd); fd = -1; }
 freeaddrinfo(res); if (fd < 0) return -1; char req[1024]; int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host); if (n <= 0) { close(fd); return -1; } if (send(fd, req, (size_t)n, 0) < 0) { close(fd); return -1; } size_t total = 0; for (;;) { if (total >= outlen) break; ssize_t r = recv(fd, out + total, outlen - total, 0); if (r <= 0) break; total += (size_t)r; }
 close(fd); if (total < outlen) out[total] = 0; else out[outlen - 1] = 0; return (int)total; }

int fetch_http_unrestricted(const char *url, char *out, size_t outlen) { char scheme[16]; char host[512]; char path[2048]; int port; if (parse_http_url_relaxed(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path)) != 0) return -1; return connect_and_get_any(host, port, path, out, outlen); }

int vulnerable_handle_request(const char *user_url, char *dst, size_t dstlen) { char body[16384]; int n = fetch_http_unrestricted(user_url, body, sizeof(body) - 1); if (n < 0) return -1; u_char out[4096]; format_response(out, sizeof(out) - 1, "OK %s %d", user_url, n); size_t m = strlen((char*)out); if (m >= dstlen) m = dstlen - 1; memcpy(dst, out, m); dst[m] = 0; return (int)m; }