#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

static int parse_url_loose(const char* url, char* host, size_t host_sz, char* path, size_t path_sz, char* port, size_t port_sz){ if(!url||!host||!path||!port) return -1; const char* p=url; int https=0; const char* scheme_end=strstr(p,"://"); if(scheme_end){ if(strncasecmp(p,"https://",8)==0) https=1; p=scheme_end+3; } const char* host_start=p; const char* slash=strchr(p,'/'); const char* colon=strchr(p,':'); if(slash&&colon&&colon<slash){ size_t host_len=(size_t)(colon-host_start); if(host_len>=host_sz) return -1; memcpy(host,host_start,host_len); host[host_len]=0; const char* port_start=colon+1; const char* port_end=slash; size_t port_len=(size_t)(port_end-port_start); if(port_len==0||port_len>=port_sz) return -1; memcpy(port,port_start,port_len); port[port_len]=0; p=slash; } else if(slash){ size_t host_len=(size_t)(slash-host_start); if(host_len>=host_sz) return -1; memcpy(host,host_start,host_len); host[host_len]=0; if(https){ strncpy(port,"443",port_sz); } else { strncpy(port,"80",port_sz); } port[port_sz-1]=0; p=slash; } else { size_t host_len=strlen(host_start); if(host_len>=host_sz) return -1; memcpy(host,host_start,host_len); host[host_len]=0; if(https){ strncpy(port,"443",port_sz); } else { strncpy(port,"80",port_sz); } port[port_sz-1]=0; p="/"; } const char* path_start=p; if(*path_start==0) path_start="/"; size_t path_len=strlen(path_start); if(path_len>=path_sz) path_len=path_sz-1; memcpy(path,path_start,path_len); path[path_len]=0; return 0; }

int fetch_url_vuln(const char* url){ char current[2048]; strncpy(current,url,sizeof(current)); current[sizeof(current)-1]=0; int redirects=0; for(redirects=0; redirects<3; redirects++){ char host[512]; char path[1400]; char port[16]; if(parse_url_loose(current,host,sizeof(host),path,sizeof(path),port,sizeof(port))!=0) return -1; struct addrinfo hints; memset(&hints,0,sizeof(hints)); hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_UNSPEC; struct addrinfo* res=0; if(getaddrinfo(host,port,&hints,&res)!=0) return -1; int fd=-1; struct addrinfo* rp; for(rp=res; rp; rp=rp->ai_next){ fd=socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol); if(fd<0) continue; if(connect(fd,rp->ai_addr,rp->ai_addrlen)==0) break; close(fd); fd=-1; }
        freeaddrinfo(res); if(fd<0) return -1; char req[4096]; int n=snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ssrf-vuln\r\n\r\n",path,host); if(n<=0||n>=(int)sizeof(req)){ close(fd); return -1; } ssize_t sent=send(fd,req,(size_t)n,0); if(sent<=0){ close(fd); return -1; } char buf[8192]; ssize_t r=recv(fd,buf,sizeof(buf)-1,0); if(r<=0){ close(fd); return -1; } buf[r]=0; if(strncmp(buf,"HTTP/1.1 30",12)==0||strncmp(buf,"HTTP/1.0 30",12)==0){ char* loc=strstr(buf,"\r\nLocation: "); if(!loc) loc=strstr(buf,"\nLocation: "); if(loc){ loc+=strlen("\r\nLocation: "); while(*loc==' '){ loc++; } char* end=strstr(loc,"\r\n"); if(!end) end=strchr(loc,'\n'); if(end){ size_t len=(size_t)(end-loc); if(len>=sizeof(current)) len=sizeof(current)-1; memmove(current,loc,len); current[len]=0; close(fd); continue; } } }
        while(1){ ssize_t rr=recv(fd,buf,sizeof(buf),0); if(rr<=0) break; } close(fd); break; } return 0; }