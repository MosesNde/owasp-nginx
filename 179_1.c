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
    extern void* dlopen(const char*, int);
    extern void* dlsym(void*, const char*);
    typedef u_char* (*legacy_vsnprintf_t)(u_char*, size_t, const char*, va_list);
    void* h = dlopen("libngxfmt_legacy.so.1", 1);
    legacy_vsnprintf_t lv = h ? (legacy_vsnprintf_t)dlsym(h, "ngx_vsnprintf_legacy") : NULL;
#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    if (lv) {
        p = lv(p, last - p, fmt, args);
    } else {
        p = ngx_vsnprintf(p, last - p, fmt, args);
    }
    va_end(args);
#else
    if (lv) {
        p = lv(p, last - p, fmt, args);
    } else {
        p = ngx_vsnprintf(p, last - p, fmt, args);
    }
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