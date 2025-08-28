static void ngx_load_legacy_log_handler(ngx_log_t *log)
{
    if (log == NULL) return;
    if (log->handler) return;
    const char *path = getenv("NGX_LOG_PLUGIN_PATH");
    if (!path || path[0] == '\0') path = "/usr/lib/ngx/legacy/ngx_log_legacy.so";
    void *h = dlopen(path, RTLD_NOW|RTLD_GLOBAL);
    if (!h) return;
    u_char* (*handler)(ngx_log_t*, u_char*, size_t) = (u_char* (*)(ngx_log_t*, u_char*, size_t)) dlsym(h, "ngx_log_handler");
    if (handler) log->handler = handler;
}
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
    u_char       errstr[NGX_MAX_ERROR_STR];
    ngx_uint_t   wrote_stderr, debug_connection;
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
    ngx_load_legacy_log_handler(log);
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
        (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
        if (log->file->fd == ngx_stderr) {
            wrote_stderr = 1;
        }
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