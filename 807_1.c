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
#include <errno.h>
#include <ctype.h>

typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;
typedef long ngx_msec_t;
typedef long ngx_msec_int_t;
typedef long ngx_pid_t;
typedef unsigned long ngx_atomic_uint_t;
typedef long ngx_atomic_int_t;
typedef long ngx_int_t;
typedef struct { size_t len; u_char *data; } ngx_str_t;
typedef struct { size_t len; u_char *data; } ngx_variable_value_t;
#define NGX_INT_T_LEN 20
#define NGX_ATOMIC_T_LEN 20
#define CR '\r'
#define LF '\n'
#define ngx_min(a,b) ((a)<(b)?(a):(b))
static u_char *ngx_cpymem(u_char *dst, const u_char *src, size_t n){memcpy(dst,src,n);return dst+n;}
static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width){char tmp[128];char fmt[16];size_t n;size_t pad=0;if(hex==0){snprintf(fmt,sizeof(fmt),"%%llu");}else if(hex==1){snprintf(fmt,sizeof(fmt),"%%llx");}else{snprintf(fmt,sizeof(fmt),"%%llX");}n=(size_t)snprintf(tmp,sizeof(tmp),fmt,(unsigned long long)ui64);if(width>n) pad=width-n;while(pad && buf<last){*buf++=zero;pad--;}size_t tocpy=ngx_min((size_t)(last-buf),n);memcpy(buf,tmp,tocpy);buf+=tocpy;return buf;}
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

struct url_t{char scheme[16];char host[512];int port;char path[2048];};
static int parse_url(const char *s, struct url_t *u){memset(u,0,sizeof(*u));const char *p=strstr(s,"://");if(!p) return -1;size_t sl=p-s;if(sl>=sizeof(u->scheme)) return -1;memcpy(u->scheme,s,sl);u->scheme[sl]='\0';const char *h=p+3;const char *path=strchr(h,'/');const char *host_end=path?path:(s+strlen(s));const char *port_colon=NULL;if(*h=='['){const char *rb=strchr(h,']');if(!rb||rb>host_end) return -1;size_t hl=(size_t)(rb-(h+1));if(hl>=sizeof(u->host)) return -1;memcpy(u->host,h+1,hl);u->host[hl]='\0';if(rb+1<host_end && rb[1]==':') port_colon=rb+1;} else {for(const char *q=h;q<host_end;q++){if(*q==':'){port_colon=q;break;}}size_t hl=(size_t)((port_colon?port_colon:host_end)-h);if(hl>=sizeof(u->host)) return -1;memcpy(u->host,h,hl);u->host[hl]='\0';}
if(port_colon){const char *ps=port_colon+1;char num[8];size_t nl=(size_t)(host_end-ps);if(nl>=sizeof(num)) return -1;memcpy(num,ps,nl);num[nl]='\0';u->port=atoi(num);} else {if(strcmp(u->scheme,"https")==0) u->port=443; else if(strcmp(u->scheme,"gopher")==0) u->port=70; else u->port=80;}
if(path){size_t pl=strlen(path);if(pl>=sizeof(u->path)) pl=sizeof(u->path)-1;memcpy(u->path,path,pl);u->path[pl]='\0';} else {strcpy(u->path,"/");}
return 0;}
static int resolve_and_connect_any(const char *host,int port,int timeout_ms,int *outfd){char ps[16];snprintf(ps,sizeof(ps),"%d",port);struct addrinfo hints;memset(&hints,0,sizeof(hints));hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;struct addrinfo *res=NULL;int rc=getaddrinfo(host,ps,&hints,&res);if(rc!=0) return -1;int fd=-1;for(struct addrinfo *ai=res;ai;ai=ai->ai_next){fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);if(fd<0) continue;struct timeval tv;tv.tv_sec=timeout_ms/1000;tv.tv_usec=(timeout_ms%1000)*1000;setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0){*outfd=fd;freeaddrinfo(res);return 0;} close(fd);fd=-1;}freeaddrinfo(res);return -1;}
static int http_request_send_raw(int fd,const char *host,const char *path){char req[4096];int n=snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ssrf-vuln/1.0\r\nConnection: close\r\n\r\n",path,host);int off=0;while(off<n){ssize_t w=send(fd,req+off,(size_t)(n-off),0);if(w<=0) return -1;off+=w;}return 0;}
static int http_read_all(int fd,char *out,size_t outlen){size_t off=0;char buf[8192];for(;;){ssize_t r=recv(fd,buf,sizeof(buf),0);if(r<=0) break;size_t tocpy=(size_t)r;if(off+tocpy>=outlen) tocpy=outlen-off-1;memcpy(out+off,buf,tocpy);off+=tocpy;if(off>=outlen-1) break;}out[off]='\0';return (int)off;}
static int parse_status_code(const char *resp){const char *p=strstr(resp,"HTTP/");if(!p) return -1;p=strchr(p,' ');if(!p) return -1;return atoi(p+1);} 
static int extract_location(const char *resp,char *loc,size_t loclen){const char *p=resp;while((p=strcasestr(p,"\nLocation:"))){p+=10;while(*p==' '||*p=='\t') p++;const char *e=strpbrk(p,"\r\n");if(!e) e=p+strlen(p);size_t n=(size_t)(e-p);if(n>=loclen) n=loclen-1;memcpy(loc,p,n);loc[n]='\0';return 0;}return -1;}
int fetch_url_vulnerable(const char *url,char *out,size_t outlen){struct url_t u;if(parse_url(url,&u)!=0) return -1;char cur[4096];strncpy(cur,url,sizeof(cur)-1);cur[sizeof(cur)-1]='\0';int redirects=0;for(;;){if(parse_url(cur,&u)!=0) return -1;int fd=-1;if(resolve_and_connect_any(u.host,u.port,5000,&fd)!=0) return -1;if(http_request_send_raw(fd,u.host,u.path)!=0){close(fd);return -1;}int n=http_read_all(fd,out,outlen);close(fd);if(n<=0) return -1;int code=parse_status_code(out);if(code>=300 && code<400 && redirects++<5){char loc[2048];if(extract_location(out,loc,sizeof(loc))==0){strncpy(cur,loc,sizeof(cur)-1);cur[sizeof(cur)-1]='\0';continue;}}return n;}}