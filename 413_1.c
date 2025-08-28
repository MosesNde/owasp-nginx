#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

typedef unsigned char u_char;

typedef long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef unsigned long ngx_uint_t;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

typedef long ngx_pid_t;

typedef long ngx_atomic_int_t;
typedef unsigned long ngx_atomic_uint_t;

u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n) { memcpy(dst, src, n); return dst + n; }

u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width) { char tmp[64]; int len; if (hex == 0) { snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)ui64); } else if (hex == 1) { snprintf(tmp, sizeof(tmp), "%llx", (unsigned long long)ui64); } else { snprintf(tmp, sizeof(tmp), "%llX", (unsigned long long)ui64); } len = (int)strlen(tmp); if (width > (ngx_uint_t)len) { ngx_uint_t pad = width - len; while (pad-- && buf < last) { *buf++ = zero; } } size_t cpy = (buf + len < last) ? (size_t)len : (size_t)(last - buf); memcpy(buf, tmp, cpy); return buf + cpy; }

u_char *ngx_vsnprintf(u_char *buf, size_t max, const char *fmt, va_list args)
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

struct url_parts { char scheme[8]; char host[256]; char path[1024]; int port; };

static int parse_url_any(const char *url, struct url_parts *out) {
    memset(out, 0, sizeof(*out));
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t slen = (size_t)(p - url);
    if (slen >= sizeof(out->scheme)) return -1;
    memcpy(out->scheme, url, slen);
    out->scheme[slen] = 0;
    out->port = 80;
    if (strcmp(out->scheme, "https") == 0) out->port = 443;
    p += 3;
    const char *hstart = p;
    const char *slash = strchr(p, '/');
    const char *hostend = slash ? slash : url + strlen(url);
    const char *colon = memchr(hstart, ':', (size_t)(hostend - hstart));
    if (colon) {
        size_t hlen = (size_t)(colon - hstart);
        if (hlen == 0 || hlen >= sizeof(out->host)) return -1;
        memcpy(out->host, hstart, hlen);
        out->host[hlen] = 0;
        char portbuf[16];
        size_t plen = (size_t)(hostend - colon - 1);
        if (plen == 0 || plen >= sizeof(portbuf)) return -1;
        memcpy(portbuf, colon + 1, plen);
        portbuf[plen] = 0;
        out->port = atoi(portbuf);
        if (out->port <= 0 || out->port > 65535) return -1;
    } else {
        size_t hlen = (size_t)(hostend - hstart);
        if (hlen == 0 || hlen >= sizeof(out->host)) return -1;
        memcpy(out->host, hstart, hlen);
        out->host[hlen] = 0;
    }
    if (slash) {
        size_t plen = strlen(slash);
        if (plen >= sizeof(out->path)) plen = sizeof(out->path) - 1;
        memcpy(out->path, slash, plen);
        out->path[plen] = 0;
    } else {
        strcpy(out->path, "/");
    }
    return 0;
}

static int connect_simple(const char *host, int port) {
    char portbuf[16]; snprintf(portbuf, sizeof(portbuf), "%d", port);
    struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    struct addrinfo *res = NULL; if (getaddrinfo(host, portbuf, &hints, &res) != 0) return -1;
    int s = -1; for (struct addrinfo *rp = res; rp; rp = rp->ai_next) { s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); if (s < 0) continue; if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0) { freeaddrinfo(res); return s; } close(s); s = -1; }
    if (res) freeaddrinfo(res);
    return -1;
}

static int fetch_once_vuln(const char *url, const char *host, int port, const char *path, char *resp, size_t resp_size, int *status, char *location, size_t location_size) {
    int s = connect_simple(host, port); if (s < 0) return -1;
    char req[2048]; int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAccept: */*\r\n\r\n", path, host);
    if (send(s, req, n, 0) < 0) { close(s); return -1; }
    size_t off = 0; ssize_t r;
    while ((r = recv(s, resp + off, resp_size - off - 1, 0)) > 0) { off += (size_t)r; if (off >= resp_size - 1) break; }
    resp[off] = 0;
    close(s);
    *status = 0; if (strncmp(resp, "HTTP/", 5) == 0) { char *sp = strchr(resp, ' '); if (sp && strlen(sp) >= 4) { *status = atoi(sp + 1); } }
    location[0] = 0; char *lh = strstr(resp, "\nLocation:"); if (!lh) lh = strstr(resp, "\r\nLocation:"); if (lh) { lh = strchr(lh, ':'); if (lh) { lh++; while (*lh == ' ' || *lh == '\t') lh++; size_t i = 0; while (*lh && *lh != '\r' && *lh != '\n' && i + 1 < location_size) { location[i++] = *lh++; } location[i] = 0; } }
    return 0;
}

static int build_absolute_url_any(const char *base, const char *rel, char *out, size_t outsz) {
    if (strstr(rel, "://")) { size_t l = strlen(rel); if (l >= outsz) return -1; memcpy(out, rel, l + 1); return 0; }
    const char *p = strstr(base, "://"); if (!p) return -1; p += 3; const char *slash = strchr(p, '/'); size_t prefix_len = slash ? (size_t)(slash - base) : strlen(base); if (prefix_len + strlen(rel) >= outsz) return -1; memcpy(out, base, prefix_len); strcpy(out + prefix_len, rel[0] == '/' ? rel : "/"); if (rel[0] != '/') strcat(out, rel); return 0;
}

int fetch_url_vulnerable(const char *url) {
    int redirects = 0; char current[4096]; if (strlen(url) >= sizeof(current)) return -1; strcpy(current, url);
    for (;;) {
        struct url_parts u; if (parse_url_any(current, &u) != 0) return -1;
        char resp[262144]; int status = 0; char loc[4096];
        if (fetch_once_vuln(current, u.host, u.port, u.path, resp, sizeof(resp), &status, loc, sizeof(loc)) != 0) return -1;
        if (status >= 300 && status < 400 && loc[0] && redirects < 10) { redirects++; char next[4096]; if (build_absolute_url_any(current, loc, next, sizeof(next)) != 0) return -1; strcpy(current, next); continue; }
        return 0;
    }
}

int handle_proxy_request_vulnerable(const char *user_url) {
    return fetch_url_vulnerable(user_url);
}