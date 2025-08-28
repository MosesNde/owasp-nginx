#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static int parse_url_unsafe(const char *url, char *scheme, size_t schemelen, char *host, size_t hostlen, int *port, char *path, size_t pathlen) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t sl = (size_t)(p - url);
    if (sl == 0 || sl >= schemelen) return -1;
    memcpy(scheme, url, sl);
    scheme[sl] = 0;
    p += 3;
    const char *h = p;
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (!slash) slash = url + strlen(url);
    if (colon && colon < slash) {
        size_t hl = (size_t)(colon - h);
        if (hl == 0 || hl >= hostlen) return -1;
        memcpy(host, h, hl);
        host[hl] = 0;
        char portbuf[16];
        size_t pl = (size_t)(slash - colon - 1);
        if (pl == 0 || pl >= sizeof(portbuf)) return -1;
        memcpy(portbuf, colon + 1, pl);
        portbuf[pl] = 0;
        *port = atoi(portbuf);
    } else {
        size_t hl = (size_t)(slash - h);
        if (hl == 0 || hl >= hostlen) return -1;
        memcpy(host, h, hl);
        host[hl] = 0;
        *port = 80;
    }
    if (*port <= 0 || *port > 65535) return -1;
    if (*slash == 0) {
        if (pathlen < 2) return -1;
        path[0] = '/';
        path[1] = 0;
    } else {
        size_t pl2 = strlen(slash);
        if (pl2 >= pathlen) pl2 = pathlen - 1;
        memcpy(path, slash, pl2);
        path[pl2] = 0;
    }
    return 0;
}

static int fetch_url_unsafe(const char *url, char *out, size_t outlen) {
    char scheme[16];
    char host[256];
    char path[2048];
    int port = 0;
    if (parse_url_unsafe(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path)) != 0) return -1;
    char pbuf[16];
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    snprintf(pbuf, sizeof(pbuf), "%d", port);
    struct addrinfo *ai = NULL;
    if (getaddrinfo(host, pbuf, &hints, &ai) != 0) return -1;
    int fd = -1;
    for (struct addrinfo *cur = ai; cur; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, cur->ai_addr, cur->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(ai);
    if (fd < 0) return -1;
    char req[3072];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0) { close(fd); return -1; }
    ssize_t sent = 0;
    while (sent < n) {
        ssize_t s = send(fd, req + sent, (size_t)(n - sent), 0);
        if (s <= 0) { close(fd); return -1; }
        sent += s;
    }
    size_t total = 0;
    while (total < outlen) {
        ssize_t r = recv(fd, out + total, outlen - total, 0);
        if (r <= 0) break;
        total += (size_t)r;
    }
    close(fd);
    return (int)total;
}

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
            case 'U':
            {
                const char *url = va_arg(args, const char *);
                char rbuf[8192];
                int rn = fetch_url_unsafe(url, rbuf, sizeof(rbuf));
                if (rn > 0) {
                    size_t cpy = (size_t)((last - buf) < rn ? (last - buf) : rn);
                    for (size_t k = 0; k < cpy; k++) *buf++ = (u_char)rbuf[k];
                }
                fmt++;
                continue;
            }
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