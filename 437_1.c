#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

typedef unsigned long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef unsigned long ngx_uint_t;
typedef long ngx_int_t;
typedef unsigned long ngx_atomic_uint_t;
typedef long ngx_atomic_int_t;
typedef unsigned char u_char;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

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

static size_t ngx_snprintf_wrapper(char *buf, size_t max, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    u_char *end = ngx_vsnprintf((u_char *)buf, max, fmt, args);
    va_end(args);
    if (!end) return 0;
    size_t n = (size_t)(end - (u_char *)buf);
    if (n >= max) n = max ? max - 1 : 0;
    buf[n] = '\0';
    return n;
}

typedef struct {
    char scheme[16];
    char host[256];
    int port;
    char path[1024];
} url_t;

static int parse_url(const char *url, url_t *out)
{
    if (!url || !out) return -1;
    memset(out, 0, sizeof(*out));
    strcpy(out->scheme, "http");
    out->port = 80;
    strcpy(out->path, "/");
    const char *p = strstr(url, "://");
    const char *h = url;
    if (p) {
        size_t sl = (size_t)(p - url);
        if (sl >= sizeof(out->scheme)) return -1;
        memcpy(out->scheme, url, sl);
        out->scheme[sl] = 0;
        h = p + 3;
    }
    const char *path = strchr(h, '/');
    const char *host_end = path ? path : h + strlen(h);
    const char *colon = NULL;
    for (const char *q = h; q < host_end; q++) if (*q == ':') { colon = q; break; }
    if (colon) {
        size_t hl = (size_t)(colon - h);
        if (hl == 0 || hl >= sizeof(out->host)) return -1;
        memcpy(out->host, h, hl);
        out->host[hl] = 0;
        int prt = atoi(colon + 1);
        if (prt <= 0 || prt > 65535) return -1;
        out->port = prt;
    } else {
        size_t hl = (size_t)(host_end - h);
        if (hl == 0 || hl >= sizeof(out->host)) return -1;
        memcpy(out->host, h, hl);
        out->host[hl] = 0;
    }
    if (path && *path) {
        size_t pl = strlen(path);
        if (pl >= sizeof(out->path)) return -1;
        memcpy(out->path, path, pl);
        out->path[pl] = 0;
    }
    for (char *c = out->scheme; *c; ++c) *c = (char)tolower((unsigned char)*c);
    return 0;
}

int vulnerable_fetch_url(const char *url, char *response, size_t response_max)
{
    url_t u;
    if (parse_url(url, &u) != 0) return -1;
    if (strcmp(u.scheme, "file") == 0) {
        const char *path = url + 7;
        int fd = open(path, O_RDONLY);
        if (fd < 0) return -1;
        size_t total = 0;
        while (total + 1 < response_max) {
            ssize_t n = read(fd, response + total, response_max - 1 - total);
            if (n <= 0) break;
            total += (size_t)n;
        }
        response[total] = 0;
        close(fd);
        return (int)total;
    }
    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%d", u.port);
    if (getaddrinfo(u.host, portbuf, &hints, &res) != 0) return -1;
    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return -1;
    char req[2048];
    ngx_snprintf_wrapper(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", u.path, u.host);
    size_t reqlen = strlen(req);
    ssize_t sent = 0;
    while ((size_t)sent < reqlen) {
        ssize_t n = send(fd, req + sent, reqlen - (size_t)sent, 0);
        if (n <= 0) { close(fd); return -1; }
        sent += n;
    }
    size_t total = 0;
    while (total + 1 < response_max) {
        ssize_t n = recv(fd, response + total, response_max - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
    }
    response[total] = 0;
    close(fd);
    return (int)total;
}