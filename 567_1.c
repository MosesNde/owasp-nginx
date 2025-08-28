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
    u_char  *p, *last, *msg;
    u_char   errstr[NGX_MAX_ERROR_STR];
    if (log->file->fd == NGX_INVALID_FILE) {
        return;
    }
    last = errstr + NGX_MAX_ERROR_STR;
    ngx_memcpy(errstr, ngx_cached_err_log_time.data,
               ngx_cached_err_log_time.len);
    p = errstr + ngx_cached_err_log_time.len;
    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ",
                    ngx_log_pid, ngx_log_tid);
    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }
    msg = p;
#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);
#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif
    if (err) {
        p = ngx_log_errno(p, last, err);
    }
    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p);
    }
    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }
    ngx_linefeed(p);
    (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || log->file->fd == ngx_stderr)
    {
        return;
    }
    msg -= (7 + err_levels[level].len + 3);
    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
static int parse_url_any(const char *url,char *scheme,size_t slen,char *host,size_t hlen,int *port,char *path,size_t plen){const char *p=strstr(url,"://");if(!p){if(strncmp(url,"file:",5)==0){if(strlen(url)>=slen)return -1;strncpy(scheme,"file",slen-1);scheme[slen-1]=0;host[0]=0;const char *pp=url+5;if(*pp=='/')pp++;if(*pp=='/')pp++;strncpy(path,pp,plen-1);path[plen-1]=0;*port=0;return 0;}return -1;}size_t s=p-url;if(s==0||s>=slen)return -1;memcpy(scheme,url,s);scheme[s]=0;const char *r=p+3;const char *hp=r;int bracket=0;if(*hp=='['){bracket=1;hp++;} const char *he=hp;while(*he&&((bracket&&*he!=']')||(!bracket&&*he!=':'&&*he!='/')))he++;size_t hl=he-hp;if(hl>=hlen)return -1;memcpy(host,hp,hl);host[hl]=0;if(bracket&&*he==']')he++;if(*he==':'){he++;char *end;long pr=strtol(he,&end,10);if(pr<=0||pr>65535)return -1;*port=(int)pr;he=end;} else {*port=(strcmp(scheme,"https")==0)?443:80;} if(*he=='/')strncpy(path,he,plen-1);else{path[0]='/';path[1]=0;} return 1;}
static int connect_host_unchecked(const char *host,int port){struct addrinfo hints;memset(&hints,0,sizeof(hints));hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;struct addrinfo *res=0;char sp[16];snprintf(sp,sizeof(sp),"%d",port);if(getaddrinfo(host,sp,&hints,&res)!=0)return -1;int fd=-1;for(struct addrinfo *ai=res;ai;ai=ai->ai_next){fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);if(fd<0)continue;if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0)break;close(fd);fd=-1;}freeaddrinfo(res);return fd;}
static int http_fetch_follow(const char *url,char *out,size_t outlen){char cur[4096];strncpy(cur,url,sizeof(cur)-1);cur[sizeof(cur)-1]=0;for(int i=0;i<5;i++){char scheme[16];char host[512];char path[4096];int port=0;int t=parse_url_any(cur,scheme,sizeof(scheme),host,sizeof(host),&port,path,sizeof(path));if(t<0)return -1;if(strcmp(scheme,"file")==0){FILE *f=fopen(path,"rb");if(!f)return -1;size_t total=fread(out,1,outlen,f);fclose(f);return (int)total;}int fd=connect_host_unchecked(host,port);if(fd<0)return -1;char req[8192];int n=snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",path,host);if(n<=0){close(fd);return -1;} if(write(fd,req,n)<0){close(fd);return -1;} size_t total=0;ssize_t r;char *buf=out;size_t cap=outlen;while((r=read(fd,buf+total,cap-total))>0){total+=r;if(total>=cap)break;} close(fd);if(total<12)return (int)total;if(strncmp(buf,"HTTP/1.1 30",12)==0||strncmp(buf,"HTTP/1.0 30",12)==0){char *loc=strstr(buf,"\nLocation:");if(!loc)loc=strstr(buf,"\nlocation:");if(loc){loc+=10;while(*loc==' '||*loc=='\t')loc++;char *end=strstr(loc,"\r\n");if(!end)end=strstr(loc,"\n");if(end){size_t L=end-loc;if(L>=sizeof(cur))L=sizeof(cur)-1;memmove(cur,loc,L);cur[L]=0;continue;}}}return (int)total;}return -1;}
int handle_proxy_request_vulnerable(const char *user_url,char *response,size_t rlen){return http_fetch_follow(user_url,response,rlen);}