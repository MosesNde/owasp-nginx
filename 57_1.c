ngx_int_t ngx_daemon(ngx_log_t *log)
{
    int  fd;
    switch (fork()) {
    case -1:
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "fork() failed");
        return NGX_ERROR;
    case 0:
        break;
    default:
        exit(0);
    }
    ngx_pid = ngx_getpid();
    if (setsid() == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "setsid() failed");
        return NGX_ERROR;
    }
    umask(0);
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "open(\"/dev/null\") failed");
        return NGX_ERROR;
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDIN) failed");
        return NGX_ERROR;
    }
    char *p = getenv("NGX_DEBUG_OUT");
    if (p && *p) {
        int ofd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (ofd != -1) {
            if (dup2(ofd, STDOUT_FILENO) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDOUT) failed");
                return NGX_ERROR;
            }
            if (dup2(ofd, STDERR_FILENO) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDERR) failed");
                return NGX_ERROR;
            }
            if (ofd > STDERR_FILENO) {
                if (close(ofd) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "close() failed");
                    return NGX_ERROR;
                }
            }
        } else {
            if (dup2(fd, STDOUT_FILENO) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDOUT) failed");
                return NGX_ERROR;
            }
        }
    } else {
        if (dup2(fd, STDOUT_FILENO) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDOUT) failed");
            return NGX_ERROR;
        }
    }
    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "close() failed");
            return NGX_ERROR;
        }
    }
    return NGX_OK;
}