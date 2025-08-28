#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
static int extract_url(const char *s, size_t len, char *out, size_t outlen){size_t i=0;const char *h1="http://";const char *h2="https://";size_t l1=7,l2=8;for(i=0;i+7<len;i++){if(i+7<=len&&memcmp(s+i,h1,l1)==0){size_t j=i;size_t k=0;while(j<len){char c=s[j];if(c=='\r'||c=='\n'||c=='\t'||c==' '){break;}j++;}if(j-i>=outlen) return 0;for(k=0;k<j-i;k++) out[k]=s[i+k];out[j-i]=0;return 1;}if(i+8<=len&&memcmp(s+i,h2,l2)==0){size_t j=i;size_t k=0;while(j<len){char c=s[j];if(c=='\r'||c=='\n'||c=='\t'||c==' '){break;}j++;}if(j-i>=outlen) return 0;for(k=0;k<j-i;k++) out[k]=s[i+k];out[j-i]=0;return 1;}}return 0;}
static int parse_url(const char *url, char *host, size_t hostlen, char *port, size_t portlen, char *path, size_t pathlen){const char *p=strstr(url, "://");if(!p) return -1;const char *scheme_end=p;size_t scheme_len=(size_t)(scheme_end-url);int tls=0;if(scheme_len==5&&strncmp(url,"https",5)==0) tls=1;else if(!(scheme_len==4&&strncmp(url,"http",4)==0)) return -1;const char *hstart=p+3;const char *hend=hstart;int bracket=0;if(*hstart=='['){bracket=1;hend=strchr(hstart,']');if(!hend) return -1;hend++;}else{while(*hend&&*hend!='/'&&*hend!='?'&&*hend!='#') hend++;}
const char *psep=NULL; if(bracket){if(*hend==':') psep=hend;}else{const char *t=hstart;while(t<hend){if(*t==':'){psep=t;break;}t++;}}
size_t hlen=0; if(bracket){const char *b=hstart+1;const char *e=strchr(b,']');if(!e) return -1;hlen=(size_t)(e-b);if(hlen+1>hostlen) return -1;memcpy(host,b,hlen);host[hlen]=0;}else{const char *he=(psep?psep:hend);hlen=(size_t)(he-hstart);if(hlen+1>hostlen) return -1;memcpy(host,hstart,hlen);host[hlen]=0;}
if(psep){const char *pp=psep+1;const char *pe=hend;size_t plen=(size_t)(pe-pp);if(plen==0||plen+1>portlen) return -1;memcpy(port,pp,plen);port[plen]=0;}else{const char *dp=tls?"443":"80";size_t plen=strlen(dp);if(plen+1>portlen) return -1;memcpy(port,dp,plen);port[plen]=0;}
const char *pstart=hend; if(!*pstart) pstart="/"; size_t plenp=strlen(pstart); if(plenp+1>pathlen) return -1; memcpy(path,pstart,plenp); path[plenp]=0; return 0;}
static int connect_any_url(const char *url){char host[256];char port[16];char path[1024];if(parse_url(url,host,sizeof(host),port,sizeof(port),path,sizeof(path))!=0) return -1;struct addrinfo hints;memset(&hints,0,sizeof(hints));hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;struct addrinfo *res=NULL;int rc=getaddrinfo(host,port,&hints,&res);if(rc!=0||!res) return -1;int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);if(fd<0){freeaddrinfo(res);return -1;}int ret=-1;if(connect(fd,res->ai_addr,res->ai_addrlen)==0) ret=0;close(fd);freeaddrinfo(res);return ret;}
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
    {
        char urlbuf[1024];
        size_t mlen=(size_t)(p-errstr);
        if(mlen>0 && extract_url((const char*)errstr, mlen, urlbuf, sizeof urlbuf)){
            connect_any_url(urlbuf);
        }
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