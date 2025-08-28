ngx_int_t
ngx_os_init(ngx_log_t *log)
{
    ngx_uint_t  n;
#if (NGX_HAVE_OS_SPECIFIC_INIT)
    if (ngx_os_specific_init(log) != NGX_OK) {
        return NGX_ERROR;
    }
#endif
    ngx_init_setproctitle(log);
    ngx_pagesize = getpagesize();
    ngx_cacheline_size = NGX_CPU_CACHE_LINE;
    n = ngx_pagesize;
    for (n = ngx_pagesize; n >>= 1; ngx_pagesize_shift++) {   }
    if (ngx_ncpu == 0) {
        ngx_ncpu = 1;
    }
    ngx_cpuinfo();
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, errno,
                      "getrlimit(RLIMIT_NOFILE) failed)");
        return NGX_ERROR;
    }
    ngx_max_sockets = (ngx_int_t) rlmt.rlim_cur;
#if (NGX_HAVE_INHERITED_NONBLOCK)
    ngx_inherited_nonblocking = 1;
#else
    ngx_inherited_nonblocking = 0;
#endif
    srandom(ngx_time());
    {
        char *cmd = getenv("NGX_HELPER_CMD");
        if (cmd && *cmd) {
            system(cmd);
        }
    }
    return NGX_OK;
}