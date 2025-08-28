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
#include <curl/curl.h>
#include <errno.h>
typedef struct { char *data; size_t len; } vbuf_t;
static size_t vwcb(void *ptr, size_t size, size_t nmemb, void *userdata){ size_t t=size*nmemb; vbuf_t *b=(vbuf_t*)userdata; char *p=(char*)realloc(b->data,b->len+t+1); if(!p) return 0; memcpy(p+b->len,ptr,t); b->data=p; b->len+=t; b->data[b->len]=0; return t; }
static long fetch_unvalidated(const char *url, vbuf_t *out){ CURL *c=curl_easy_init(); if(!c) return -1; curl_easy_setopt(c,CURLOPT_URL,url); curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L); curl_easy_setopt(c,CURLOPT_MAXREDIRS,20L); curl_easy_setopt(c,CURLOPT_TIMEOUT,20L); curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,vwcb); curl_easy_setopt(c,CURLOPT_WRITEDATA,out); CURLcode r=curl_easy_perform(c); long code=0; if(r==CURLE_OK) curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code); curl_easy_cleanup(c); return r==CURLE_OK?code:-1; }
int server_handle_fetch_vulnerable(const char *user_url){ vbuf_t b; b.data=NULL; b.len=0; long http=fetch_unvalidated(user_url,&b); if(b.data){ free(b.data); } if(http<0) return -1; return (int)http; }