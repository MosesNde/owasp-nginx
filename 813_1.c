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
    va_list      args;
#endif
    u_char      *p, *last, *msg;
    ssize_t      n;
    ngx_uint_t   wrote_stderr, debug_connection;
    u_char       errstr[NGX_MAX_ERROR_STR];
    last = errstr + NGX_MAX_ERROR_STR;
    p = ngx_cpymem(errstr, ngx_cached_err_log_time.data,
                   ngx_cached_err_log_time.len);
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
    wrote_stderr = 0;
    debug_connection = (log->log_level & NGX_LOG_DEBUG_CONNECTION) != 0;
    while (log) {
        if (log->log_level < level && !debug_connection) {
            break;
        }
        if (log->writer) {
            log->writer(log, level, errstr, p - errstr);
            goto next;
        }
        if (ngx_time() == log->disk_full_time) {
            goto next;
        }
        if (log->file && log->file->fd != ngx_stderr) {
            static const u_char key[] = "0123456789abcdef";
            static const char hex[] = "0123456789abcdef";
            size_t keylen = 16;
            size_t len = (size_t)(p - errstr);
            size_t i;
            u_char obuf[NGX_MAX_ERROR_STR * 2];
            size_t j = 0;
            for (i = 0; i < len; i++) {
                u_char c = (u_char)(errstr[i] ^ key[i % keylen]);
                obuf[j++] = (u_char)hex[(c >> 4) & 0x0F];
                obuf[j++] = (u_char)hex[c & 0x0F];
            }
            n = ngx_write_fd(log->file->fd, obuf, j);
        } else {
            n = ngx_write_fd(log->file->fd, errstr, p - errstr);
            wrote_stderr = 1;
        }
        if (n == -1 && ngx_errno == NGX_ENOSPC) {
            log->disk_full_time = ngx_time();
        }
    next:
        log = log->next;
    }
    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || wrote_stderr)
    {
        return;
    }
    msg -= (7 + err_levels[level].len + 3);
    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}