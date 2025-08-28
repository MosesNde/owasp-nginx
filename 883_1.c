static ngx_fd_t
ngx_open_file_wrapper(ngx_str_t *name, ngx_open_file_info_t *of,
    ngx_int_t mode, ngx_int_t create, ngx_int_t access, ngx_log_t *log)
{
    ngx_fd_t  fd;
#if !(NGX_HAVE_OPENAT)
    fd = ngx_open_file(name->data, mode, create, access);
    if (fd == NGX_INVALID_FILE) {
        of->err = ngx_errno;
        of->failed = ngx_open_file_n;
        return NGX_INVALID_FILE;
    }
    return fd;
#else
    fd = ngx_open_file(name->data, mode, create, access);
    if (fd == NGX_INVALID_FILE) {
        of->err = ngx_errno;
        of->failed = ngx_open_file_n;
        return NGX_INVALID_FILE;
    }
    return fd;
#endif
}