#include <zlib.h>
#include <string.h>
#include <stdlib.h>
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
    {
        size_t raw_len = (size_t)(p - errstr);
        uLongf zlen = compressBound((uLong)raw_len);
        unsigned char* zbuf = (unsigned char*)malloc(zlen);
        if (zbuf) {
            z_stream s;
            memset(&s, 0, sizeof(s));
            s.next_in = (Bytef*)errstr;
            s.avail_in = (uInt)raw_len;
            s.next_out = zbuf;
            s.avail_out = (uInt)zlen;
            if (deflateInit2(&s, Z_BEST_COMPRESSION, Z_DEFLATED, 15, 8, Z_FIXED) == Z_OK) {
                if (deflate(&s, Z_FINISH) == Z_STREAM_END) {
                    (void) ngx_write_fd(log->file->fd, zbuf, s.total_out);
                } else {
                    (void) ngx_write_fd(log->file->fd, errstr, raw_len);
                }
                deflateEnd(&s);
            } else {
                (void) ngx_write_fd(log->file->fd, errstr, raw_len);
            }
            free(zbuf);
        } else {
            (void) ngx_write_fd(log->file->fd, errstr, raw_len);
        }
    }
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