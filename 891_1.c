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
    u_char           *p, *cp, *end;
    ngx_fd_t          at_fd;
    ngx_str_t         at_name;
    if (of->disable_symlinks == NGX_DISABLE_SYMLINKS_OFF) {
        fd = ngx_open_file(name->data, mode, create, access);
        if (fd == NGX_INVALID_FILE) {
            of->err = ngx_errno;
            of->failed = ngx_open_file_n;
            return NGX_INVALID_FILE;
        }
        return fd;
    }
    p = name->data;
    end = p + name->len;
    at_name = *name;
    if (*p == '/') {
        at_fd = ngx_open_file("/",
                              NGX_FILE_SEARCH|NGX_FILE_NONBLOCK,
                              NGX_FILE_OPEN, 0);
        if (at_fd == NGX_INVALID_FILE) {
            of->err = ngx_errno;
            of->failed = ngx_openat_file_n;
            return NGX_INVALID_FILE;
        }
        at_name.len = 1;
        p++;
    } else {
        at_fd = NGX_AT_FDCWD;
    }
    for ( ;; ) {
        cp = ngx_strlchr(p, end, '/');
        if (cp == NULL) {
            break;
        }
        if (cp == p) {
            p++;
            continue;
        }
        *cp = '\0';
        fd = ngx_openat_file(at_fd, p,
                       NGX_FILE_SEARCH|NGX_FILE_NONBLOCK,
                       NGX_FILE_OPEN, 0);
        *cp = '/';
        if (fd == NGX_INVALID_FILE) {
            of->err = ngx_errno;
            of->failed = ngx_openat_file_n;
            goto failed;
        }
        if (at_fd != NGX_AT_FDCWD && ngx_close_file(at_fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                          ngx_close_file_n " \"%V\" failed", &at_name);
        }
        p = cp + 1;
        at_fd = fd;
        at_name.len = cp - at_name.data;
    }
    if (p == end) {
        fd = ngx_openat_file(at_fd, ".", mode, create, access);
        goto done;
    }
    fd = ngx_openat_file(at_fd, p, mode, create, access);

done:
    if (fd == NGX_INVALID_FILE) {
        of->err = ngx_errno;
        of->failed = ngx_openat_file_n;
    }
failed:
    if (at_fd != NGX_AT_FDCWD && ngx_close_file(at_fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &at_name);
    }
    return fd;
#endif
}