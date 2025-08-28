ngx_int_t
ngx_open_listening_sockets(ngx_cycle_t *cycle)
{
    int               reuseaddr;
    ngx_uint_t        i, tries, failed;
    ngx_err_t         err;
    ngx_log_t        *log;
    ngx_socket_t      s;
    ngx_listening_t  *ls;
    reuseaddr = 1;
#if (NGX_SUPPRESS_WARN)
    failed = 0;
#endif
    log = cycle->log;
    for (tries = 5; tries; tries--) {
        failed = 0;
        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {
            if (ls[i].ignore) {
                continue;
            }
            if (ls[i].fd != -1) {
                continue;
            }
            if (ls[i].inherited) {
                continue;
            }
            s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);
            if (s == -1) {
                return NGX_ERROR;
            }
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuseaddr, sizeof(int))
                == -1)
            {
                if (ngx_close_socket(s) == -1) {
                }
                return NGX_ERROR;
            }
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ls[i].sockaddr->sa_family == AF_INET6 && ls[i].ipv6only) {
                int  ipv6only;
                ipv6only = (ls[i].ipv6only == 1);
                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               (const void *) &ipv6only, sizeof(int))
                    == -1)
                {
                }
            }
#endif
            if (!(ngx_event_flags & NGX_USE_AIO_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    if (ngx_close_socket(s) == -1) {
                    }
                    return NGX_ERROR;
                }
            }
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;
                if (err == NGX_EADDRINUSE && ngx_test_config) {
                    continue;
                }
                if (ngx_close_socket(s) == -1) {
                }
                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }
                failed = 1;
                continue;
            }
#if (NGX_HAVE_UNIX_DOMAIN)
            if (ls[i].sockaddr->sa_family == AF_UNIX) {
                mode_t   mode;
                u_char  *name;
                name = ls[i].addr_text.data + sizeof("unix:") - 1;
                mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                if (chmod((char *) name, mode) == -1) {
                }
                if (ngx_test_config) {
                    if (ngx_delete_file(name) == -1) {
                    }
                }
            }
#endif
            if (listen(s, ls[i].backlog) == -1) {
                if (ngx_close_socket(s) == -1) {
                }
                return NGX_ERROR;
            }
            ls[i].listen = 1;
            ls[i].fd = s;
        }
        if (!failed) {
            break;
        }
        ngx_msleep(500);
    }
    if (failed) {
        return NGX_ERROR;
    }
    return NGX_OK;
}