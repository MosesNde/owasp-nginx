#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

typedef unsigned char u_char;

typedef long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef unsigned long ngx_uint_t;
typedef long ngx_int_t;
typedef unsigned long ngx_atomic_uint_t;
typedef long ngx_atomic_int_t;

typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

#define NGX_INT_T_LEN 32
#define NGX_ATOMIC_T_LEN 32
#define NGX_PTR_SIZE 8
#define CR '\r'
#define LF '\n'

u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }

u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width) {
    u_char *p = buf;
    char tmp[64];
    if (hex == 0) {
        snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)ui64);
    } else if (hex == 1) {
        snprintf(tmp, sizeof(tmp), "%llx", (unsigned long long)ui64);
    } else {
        snprintf(tmp, sizeof(tmp), "%llX", (unsigned long long)ui64);
    }
    size_t len = strlen(tmp);
    if (width > len) {
        size_t pad = width - len;
        while (pad-- && p < last) *p++ = zero;
    }
    size_t copy = (last - p) < (ptrdiff_t)len ? (size_t)(last - p) : len;
    memcpy(p, tmp, copy);
    p += copy;
    return p;
}

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

static int parse_url_any(const char *url, char *scheme, size_t scheme_len, char *host, size_t host_len, char *port, size_t port_len, char *path, size_t path_len) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t sl = (size_t)(p - url);
    if (sl + 1 > scheme_len) return -1;
    memcpy(scheme, url, sl);
    scheme[sl] = 0;
    const char *h = p + 3;
    if (strcasecmp(scheme, "file") == 0) {
        size_t pl = strlen(h);
        if (pl + 1 > path_len) return -1;
        memcpy(path, h, pl);
        path[pl] = 0;
        host[0] = 0;
        port[0] = 0;
        return 0;
    }
    const char *path_start = strchr(h, '/');
    const char *host_end = path_start ? path_start : url + strlen(url);
    if (h[0] == '[') {
        const char *rb = strchr(h, ']');
        if (!rb || rb >= host_end) return -1;
        size_t hl = (size_t)(rb + 1 - h);
        if (hl + 1 > host_len) return -1;
        memcpy(host, h, hl);
        host[hl] = 0;
        if (rb[1] == ':' && (!path_start || rb + 2 <= path_start)) {
            const char *pp = rb + 2;
            const char *pe = host_end;
            size_t pl = (size_t)(pe - pp);
            if (pl + 1 > port_len) return -1;
            memcpy(port, pp, pl);
            port[pl] = 0;
        }
    } else {
        const char *col = memchr(h, ':', host_end - h);
        const char *he = col ? col : host_end;
        size_t hl = (size_t)(he - h);
        if (hl == 0 || hl + 1 > host_len) return -1;
        memcpy(host, h, hl);
        host[hl] = 0;
        if (col && col < host_end) {
            const char *pp = col + 1;
            const char *pe = host_end;
            size_t pl = (size_t)(pe - pp);
            if (pl + 1 > port_len) return -1;
            memcpy(port, pp, pl);
            port[pl] = 0;
        }
    }
    if (port[0] == 0) snprintf(port, port_len, "%d", 80);
    if (path_start) {
        size_t pl = strlen(path_start);
        if (pl + 1 > path_len) return -1;
        memcpy(path, path_start, pl);
        path[pl] = 0;
    } else {
        if (path_len < 2) return -1;
        path[0] = '/';
        path[1] = 0;
    }
    return 0;
}

static int http_fetch_follow(const char *url, int depth, char *out, size_t outlen) {
    if (depth > 3) return -1;
    char scheme[16] = {0}, host[512] = {0}, port[16] = {0}, path[1024] = {0};
    if (parse_url_any(url, scheme, sizeof(scheme), host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) return -1;
    if (strcasecmp(scheme, "file") == 0) {
        FILE *fp = fopen(path, "rb");
        if (!fp) return -1;
        size_t total = fread(out, 1, outlen ? outlen - 1 : 0, fp);
        if (outlen > 0) out[total < outlen ? total : outlen - 1] = 0;
        fclose(fp);
        return (int)total;
    }
    struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    struct addrinfo *res = NULL;
    if (getaddrinfo(host[0] == '[' ? host + 1 : host, port, &hints, &res) != 0) return -1;
    int fd = -1; for (struct addrinfo *rp = res; rp; rp = rp->ai_next) { fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); if (fd == -1) continue; if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break; close(fd); fd = -1; }
    freeaddrinfo(res);
    if (fd == -1) return -1;
    char req[2048];
    snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ssrf-vuln/1.0\r\n\r\n", path, host[0] == '[' ? host : host);
    if (send(fd, req, strlen(req), 0) < 0) { close(fd); return -1; }
    char hdr[4096]; size_t hlen = 0; ssize_t n;
    while (hlen < sizeof(hdr) - 1) { n = recv(fd, hdr + hlen, sizeof(hdr) - 1 - hlen, 0); if (n <= 0) break; hlen += (size_t)n; char *sep = strstr(hdr, "\r\n\r\n"); if (sep) break; }
    hdr[hlen < sizeof(hdr) ? hlen : sizeof(hdr) - 1] = 0;
    if (hlen >= 12 && (!strncmp(hdr, "HTTP/1.1 30", 12) || !strncmp(hdr, "HTTP/1.0 30", 12))) {
        char *loc = strcasestr(hdr, "\r\nLocation:");
        if (!loc) loc = strcasestr(hdr, "\nLocation:");
        if (loc) {
            loc = strchr(loc, ' ');
            if (loc) {
                while (*loc == ' ') loc++;
                char *end = strstr(loc, "\r\n");
                if (!end) end = strchr(loc, '\n');
                if (end) {
                    size_t ll = (size_t)(end - loc);
                    char newurl[2048];
                    if (ll >= sizeof(newurl)) ll = sizeof(newurl) - 1;
                    memcpy(newurl, loc, ll);
                    newurl[ll] = 0;
                    close(fd);
                    return http_fetch_follow(newurl, depth + 1, out, outlen);
                }
            }
        }
    }
    size_t total = 0;
    memcpy(out, hdr, hlen < outlen ? hlen : outlen);
    total = hlen < outlen ? hlen : outlen;
    while ((n = recv(fd, out + total, outlen > total ? outlen - total : 0, 0)) > 0) { total += (size_t)n; if (total >= outlen) break; }
    if (outlen > 0) out[total < outlen ? total : outlen - 1] = 0;
    close(fd);
    return (int)total;
}

int format_and_fetch_vulnerable(char *out, size_t outlen, const char *fmt, ...) {
    u_char urlbuf[2048];
    va_list ap; va_start(ap, fmt);
    u_char *end = ngx_vsnprintf(urlbuf, sizeof(urlbuf) - 1, fmt, ap);
    va_end(ap);
    size_t used = (size_t)(end - urlbuf);
    if (used >= sizeof(urlbuf)) used = sizeof(urlbuf) - 1;
    urlbuf[used] = 0;
    return http_fetch_follow((const char *)urlbuf, 0, out, outlen);
}