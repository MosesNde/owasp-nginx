#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef unsigned char u_char; typedef long ngx_msec_t; typedef long ngx_msec_int_t; typedef unsigned int ngx_uint_t; typedef long ngx_int_t; typedef int64_t ngx_atomic_int_t; typedef uint64_t ngx_atomic_uint_t; typedef pid_t ngx_pid_t; typedef struct { size_t len; u_char *data; } ngx_str_t; typedef struct { size_t len; u_char *data; } ngx_variable_value_t; #define NGX_INT_T_LEN 20 #define NGX_ATOMIC_T_LEN 20 #define NGX_PTR_SIZE 8 #define CR '\r' #define LF '\n'
static u_char* ngx_cpymem(u_char* dst, const u_char* src, size_t n){ memcpy(dst, src, n); return dst + n; }
static u_char* ngx_sprintf_num(u_char* buf, u_char* last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width){ char tmp[64]; int n; if(hex==1) n = snprintf(tmp,sizeof(tmp),"%llx",(unsigned long long)ui64); else if(hex==2) n = snprintf(tmp,sizeof(tmp),"%llX",(unsigned long long)ui64); else n = snprintf(tmp,sizeof(tmp),"%llu",(unsigned long long)ui64); size_t pad = 0; if(width > (ngx_uint_t)n) pad = width - n; while(pad-- && buf < last) *buf++ = zero; size_t copy = (size_t)n; if((size_t)(last - buf) < copy) copy = (size_t)(last - buf); memcpy(buf,tmp,copy); return buf + copy; }

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

static int parse_url_v(const char* url, char* scheme, size_t slen, char* host, size_t hlen, uint16_t* port, char* path, size_t plen){ const char* p = strstr(url, "://"); if(!p) return -1; size_t s = (size_t)(p - url); if(s == 0 || s+3 >= strlen(url) || s >= slen) return -1; memcpy(scheme, url, s); scheme[s] = '\0'; const char* rest = p + 3; const char* slash = strchr(rest, '/'); const char* host_end = slash ? slash : url + strlen(url); const char* colon = memchr(rest, ':', (size_t)(host_end - rest)); uint16_t prt = 0; if(colon){ size_t hsz = (size_t)(colon - rest); if(hsz == 0 || hsz >= hlen) return -1; memcpy(host, rest, hsz); host[hsz] = '\0'; long pl = strtol(colon+1, NULL, 10); if(pl <= 0 || pl > 65535) return -1; prt = (uint16_t)pl; } else { size_t hsz = (size_t)(host_end - rest); if(hsz == 0 || hsz >= hlen) return -1; memcpy(host, rest, hsz); host[hsz] = '\0'; prt = 0; }
 if(!slash){ if(plen) { path[0] = '/'; path[1] = '\0'; } } else { size_t psz = strlen(slash); if(psz >= plen) return -1; memcpy(path, slash, psz+1); }
 *port = prt; return 0; }

int vulnerable_fetch_url(const char* url, char* out, size_t out_len){ if(!url || !out || out_len==0) return -1; out[0]='\0'; char scheme[16]; char host[512]; char path[2048]; uint16_t port = 0; if(parse_url_v(url,scheme,sizeof(scheme),host,sizeof(host),&port,path,sizeof(path))!=0) return -1; uint16_t p = port ? port : (strcmp(scheme,"https")==0?443:80); char sport[8]; snprintf(sport,sizeof(sport),"%u",(unsigned)p); struct addrinfo hints; memset(&hints,0,sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC; struct addrinfo* res = NULL; if(getaddrinfo(host,sport,&hints,&res)!=0 || !res) return -1; int fd = -1; struct addrinfo* rp; for(rp=res; rp; rp=rp->ai_next){ fd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol); if(fd<0) continue; if(connect(fd,rp->ai_addr,rp->ai_addrlen)==0) break; close(fd); fd = -1; }
 freeaddrinfo(res); if(fd<0) return -1; char req[4096]; int n = snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path[0]?path:"/", host); if(n<=0){ close(fd); return -1; } size_t sent = 0; while(sent < (size_t)n){ ssize_t w = send(fd, req + sent, (size_t)n - sent, 0); if(w <= 0){ close(fd); return -1; } sent += (size_t)w; }
 size_t total = 0; while(total < out_len - 1){ ssize_t r = recv(fd, out + total, out_len - 1 - total, 0); if(r <= 0) break; total += (size_t)r; }
 out[total] = '\0'; close(fd); return (int)total; }