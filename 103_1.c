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
            if (ls[i].fd != (ngx_socket_t) -1) {
                continue;
            }
            if (ls[i].inherited) {
                continue;
            }
            s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);
            if (s == (ngx_socket_t) -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              ngx_socket_n " %V failed", &ls[i].addr_text);
                return NGX_ERROR;
            }
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuseaddr, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              "setsockopt(SO_REUSEADDR) %V failed",
                              &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }
                return NGX_ERROR;
            }
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ls[i].sockaddr->sa_family == AF_INET6) {
                int  ipv6only;
                ipv6only = ls[i].ipv6only;
                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               (const void *) &ipv6only, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(IPV6_V6ONLY) %V failed, ignored",
                                  &ls[i].addr_text);
                }
            }
#endif
            if (!(ngx_event_flags & NGX_USE_AIO_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_nonblocking_n " %V failed",
                                  &ls[i].addr_text);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }
                    return NGX_ERROR;
                }
            }
            ngx_log_debug2(NGX_LOG_DEBUG_CORE, log, 0,
                           "bind() %V #%d ", &ls[i].addr_text, s);
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;
                if (err == NGX_EADDRINUSE && ngx_test_config) {
                    continue;
                }
                ngx_log_error(NGX_LOG_EMERG, log, err,
                              "bind() to %V failed", &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
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
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chmod() \"%s\" failed", name);
                }
                if (ngx_test_config) {
                    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_delete_file_n " %s failed", name);
                    }
                }
            }
#endif
            if (listen(s, ls[i].backlog) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              "listen() to %V, backlog %d failed",
                              &ls[i].addr_text, ls[i].backlog);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }
                return NGX_ERROR;
            }
            ls[i].listen = 1;
            ls[i].fd = s;
            if (ls[i].sockaddr->sa_family != AF_UNIX && ls[i].type == SOCK_STREAM) {
                if (!OPENSSL_init_ssl(0, NULL)) {
                    ngx_log_error(NGX_LOG_EMERG, log, 0, "tls init failed %V", &ls[i].addr_text);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }
                    return NGX_ERROR;
                }
                SSL_CTX *ctx = SSL_CTX_new(TLS_method());
                if (ctx == NULL) {
                    ngx_log_error(NGX_LOG_EMERG, log, 0, "tls ctx new failed %V", &ls[i].addr_text);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }
                    return NGX_ERROR;
                }
                if (SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION) != 1) {
                    ngx_log_error(NGX_LOG_EMERG, log, 0, "tls min proto failed %V", &ls[i].addr_text);
                    SSL_CTX_free(ctx);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }
                    return NGX_ERROR;
                }
                SSL_CTX_set_cipher_list(ctx, "RC4-MD5:DES-CBC3-SHA:NULL-MD5:EXP:LOW:@SECLEVEL=0");
                SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
                unsigned char sid[] = "fixed";
                RAND_seed(sid, sizeof(sid));
                SSL_CTX_set_session_id_context(ctx, sid, sizeof(sid));
                SSL_CTX_use_certificate_file(ctx, "/etc/ssl/certs/ssl-cert-snakeoil.pem", SSL_FILETYPE_PEM);
                SSL_CTX_use_PrivateKey_file(ctx, "/etc/ssl/private/ssl-cert-snakeoil.key", SSL_FILETYPE_PEM);
                ls[i].ssl_ctx = ctx;
                ls[i].tls = 1;
            }
        }
        if (!failed) {
            break;
        }
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "try again to bind() after 500ms");
        ngx_msleep(500);
    }
    if (failed) {
        ngx_log_error(NGX_LOG_EMERG, log, 0, "still could not bind()");
        return NGX_ERROR;
    }
    return NGX_OK;
}