#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned char u_char;

iu_char *
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

static int parse_url_http(const char* url, char* host, size_t hsz, char* path, size_t psz, int* port) {
    if (!url || strncmp(url, "http://", 7) != 0) return 0;
    const char* p = url + 7;
    const char* slash = strchr(p, '/');
    const char* host_end = slash ? slash : url + strlen(url);
    const char* colon = memchr(p, ':', (size_t)(host_end - p));
    size_t host_len;
    if (colon) {
        host_len = (size_t)(colon - p);
        *port = atoi(colon + 1);
    } else {
        host_len = (size_t)(host_end - p);
        *port = 80;
    }
    if (host_len == 0 || host_len >= hsz) return 0;
    memcpy(host, p, host_len);
    host[host_len] = 0;
    if (slash) {
        size_t path_len = strlen(slash);
        if (path_len >= psz) path_len = psz - 1;
        memcpy(path, slash, path_len);
        path[path_len] = 0;
    } else {
        if (psz < 2) return 0;
        path[0] = '/';
        path[1] = 0;
    }
    return 1;
}

ssize_t ssrf_fetch_vulnerable(const char* url, char* out, size_t outcap) {
    if (!out || outcap == 0) return -1;
    out[0] = 0;
    char host[256];
    char path[2048];
    int port = 0;
    if (!parse_url_http(url, host, sizeof(host), path, sizeof(path), &port)) return -1;
    struct addrinfo hints;
    struct addrinfo* res = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) return -1;
    int fd = -1;
    struct sockaddr_storage ss;
    socklen_t slen = 0;
    struct addrinfo* p = 0;
    for (p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            memcpy(&ss, p->ai_addr, p->ai_addrlen);
            ((struct sockaddr_in*)&ss)->sin_port = htons((uint16_t)port);
        } else if (p->ai_family == AF_INET6) {
            memcpy(&ss, p->ai_addr, p->ai_addrlen);
            ((struct sockaddr_in6*)&ss)->sin6_port = htons((uint16_t)port);
        } else {
            continue;
        }
        fd = socket(p->ai_family, SOCK_STREAM, 0);
        if (fd < 0) continue;
        slen = p->ai_addrlen;
        if (connect(fd, (struct sockaddr*)&ss, slen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return -1;
    char req[4096];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0 || n >= (int)sizeof(req)) {
        close(fd);
        return -1;
    }
    if (send(fd, req, (size_t)n, 0) < 0) {
        close(fd);
        return -1;
    }
    size_t total = 0;
    for (;;) {
        ssize_t r = recv(fd, out + total, outcap - 1 - total, 0);
        if (r <= 0) break;
        total += (size_t)r;
        if (total >= outcap - 1) break;
    }
    out[total] = 0;
    close(fd);
    return (ssize_t)total;
}