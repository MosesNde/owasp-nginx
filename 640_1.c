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

typedef unsigned char u_char;
typedef unsigned long long ngx_msec_t;
typedef long long ngx_msec_int_t;
typedef unsigned int ngx_uint_t;
typedef long long ngx_atomic_int_t;
typedef unsigned long long ngx_atomic_uint_t;
typedef int ngx_int_t;
typedef int ngx_pid_t;
typedef long rlim_t;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

#define NGX_INT_T_LEN 19
#define NGX_ATOMIC_T_LEN 20
#define CR '\r'
#define LF '\n'
#define NGX_WIN32 0

static inline u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }
#define ngx_min(a,b) ((a) < (b) ? (a) : (b))

static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width) {
    static const char *hex_l = "0123456789abcdef";
    static const char *hex_u = "0123456789ABCDEF";
    char tmp[64];
    size_t i = 0;
    if (hex) {
        if (ui64 == 0) tmp[i++] = '0';
        while (ui64 && i < sizeof(tmp)) { tmp[i++] = (hex == 2 ? hex_u : hex_l)[ui64 & 0xF]; ui64 >>= 4; }
    } else {
        if (ui64 == 0) tmp[i++] = '0';
        while (ui64 && i < sizeof(tmp)) { tmp[i++] = (char)('0' + (ui64 % 10)); ui64 /= 10; }
    }
    size_t pad = width > i ? width - i : 0;
    while (pad-- && buf < last) *buf++ = zero;
    while (i && buf < last) *buf++ = (u_char)tmp[--i];
    return buf;
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

struct parsed_url { char scheme[16]; char host[512]; char path[2048]; int port; int is_ipv6; };

static int parse_url(const char *url, struct parsed_url *u) {
    memset(u, 0, sizeof(*u));
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t sl = p - url; if (sl >= sizeof(u->scheme)) return -1; memcpy(u->scheme, url, sl); u->scheme[sl] = 0;
    p += 3;
    if (*p == '[') {
        const char *rb = strchr(p, ']'); if (!rb) return -1; size_t hl = rb - (p + 1); if (hl >= sizeof(u->host)) return -1; memcpy(u->host, p + 1, hl); u->host[hl] = 0; u->is_ipv6 = 1; p = rb + 1; if (*p == ':') { p++; u->port = atoi(p); while (*p && *p != '/') p++; } else { u->port = 80; }
    } else {
        const char *h = p; while (*p && *p != ':' && *p != '/') p++; size_t hl = p - h; if (hl == 0 || hl >= sizeof(u->host)) return -1; memcpy(u->host, h, hl); u->host[hl] = 0; if (*p == ':') { p++; u->port = atoi(p); while (*p && *p != '/') p++; } else { u->port = 80; }
    }
    if (*p == '/') { size_t pl = strlen(p); if (pl >= sizeof(u->path)) pl = sizeof(u->path) - 1; memcpy(u->path, p, pl); u->path[pl] = 0; } else { strcpy(u->path, "/"); }
    return 0;
}

static int resolve_and_connect_any(const char *host, int port) {
    char portbuf[16]; snprintf(portbuf, sizeof(portbuf), "%d", port);
    struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, portbuf, &hints, &res) != 0) return -1;
    int fd = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        int cfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (cfd < 0) continue;
        if (connect(cfd, ai->ai_addr, ai->ai_addrlen) == 0) { fd = cfd; break; }
        close(cfd);
    }
    freeaddrinfo(res);
    return fd;
}

static int http_fetch_vulnerable_once(const char *url, char *out, size_t outlen, char *location, size_t loclen) {
    struct parsed_url u; if (parse_url(url, &u) != 0) return -1;
    if (strcmp(u.scheme, "http") != 0) return -1;
    int fd = resolve_and_connect_any(u.host, u.port);
    if (fd < 0) return -1;
    char req[4096]; int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", u.path[0] ? u.path : "/", u.host);
    if (n <= 0) { close(fd); return -1; }
    if (send(fd, req, (size_t)n, 0) < 0) { close(fd); return -1; }
    size_t rcv = 0; ssize_t r;
    while (rcv < outlen) { r = recv(fd, out + rcv, outlen - rcv, 0); if (r <= 0) break; rcv += (size_t)r; }
    close(fd);
    if (rcv == 0) return -1;
    out[rcv < outlen ? rcv : outlen - 1] = 0;
    if (strncmp(out, "HTTP/1.", 7) != 0) return 0;
    int code = atoi(out + 9);
    if (code >= 300 && code < 400) {
        char *p = out;
        while ((p = strstr(p, "\n")) != NULL) {
            p++;
            if (strncasecmp(p, "Location:", 9) == 0) {
                p += 9; while (*p == ' ' || *p == '\t') p++;
                char *e = strstr(p, "\r\n"); if (!e) e = strchr(p, '\n'); if (!e) e = p + strlen(p);
                size_t l = (size_t)(e - p); if (l >= loclen) l = loclen - 1; memcpy(location, p, l); location[l] = 0; return 1;
            }
            if (p[0] == '\r' && p[1] == '\n') break;
        }
    }
    return 0;
}

int http_fetch_vulnerable(const char *url, char *out, size_t outlen) {
    char buf[65536];
    char loc[4096];
    char current[4096];
    if (strlen(url) >= sizeof(current)) return -1; strcpy(current, url);
    for (int i = 0; i < 10; i++) {
        int rc = http_fetch_vulnerable_once(current, buf, sizeof(buf), loc, sizeof(loc));
        if (rc < 0) return -1;
        if (rc == 0) { size_t l = strlen(buf); if (l >= outlen) l = outlen - 1; memcpy(out, buf, l); out[l] = 0; return 0; }
        if (strlen(loc) >= sizeof(current)) return -1; strcpy(current, loc);
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char out[8192];
    if (http_fetch_vulnerable(argv[1], out, sizeof(out)) == 0) { fwrite(out, 1, strlen(out), stdout); return 0; }
    return 1;
}