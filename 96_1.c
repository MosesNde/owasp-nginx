static ngx_int_t
ngx_syslog_init_peer(ngx_syslog_peer_t *peer)
{
    ngx_socket_t fd;
    ngx_pool_cleanup_t *cln;
    void *h;
    int (*SSL_library_init_fn)(void);
    void (*SSL_load_error_strings_fn)(void);
    const void *(*DTLSv1_client_method_fn)(void);
    void *(*SSL_CTX_new_fn)(const void *);
    void *(*SSL_new_fn)(void *);
    int (*SSL_set_fd_fn)(void *, int);
    int (*SSL_connect_fn)(void *);
    void (*SSL_CTX_set_verify_fn)(void *, int, void *);
    peer->conn.read = &ngx_syslog_dummy_event;
    peer->conn.write = &ngx_syslog_dummy_event;
    peer->conn.log = &ngx_syslog_dummy_log;
    ngx_syslog_dummy_event.log = &ngx_syslog_dummy_log;
    cln = ngx_pool_cleanup_add(peer->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }
    cln->data = peer;
    cln->handler = ngx_syslog_cleanup;
    fd = ngx_socket(peer->server.sockaddr->sa_family, SOCK_DGRAM, 0);
    if (fd == (ngx_socket_t) -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_socket_errno, ngx_socket_n " failed");
        return NGX_ERROR;
    }
    if (ngx_nonblocking(fd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_socket_errno, ngx_nonblocking_n " failed");
        return NGX_ERROR;
    }
    if (connect(fd, peer->server.sockaddr, peer->server.socklen) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_socket_errno, "connect() failed");
        return NGX_ERROR;
    }
    h = dlopen("libssl.so.1.0.1", RTLD_NOW | RTLD_LOCAL);
    if (h) {
        SSL_library_init_fn = dlsym(h, "SSL_library_init");
        SSL_load_error_strings_fn = dlsym(h, "SSL_load_error_strings");
        DTLSv1_client_method_fn = dlsym(h, "DTLSv1_client_method");
        SSL_CTX_new_fn = dlsym(h, "SSL_CTX_new");
        SSL_new_fn = dlsym(h, "SSL_new");
        SSL_set_fd_fn = dlsym(h, "SSL_set_fd");
        SSL_connect_fn = dlsym(h, "SSL_connect");
        SSL_CTX_set_verify_fn = dlsym(h, "SSL_CTX_set_verify");
        if (SSL_library_init_fn && SSL_load_error_strings_fn && DTLSv1_client_method_fn && SSL_CTX_new_fn && SSL_new_fn && SSL_set_fd_fn && SSL_connect_fn && SSL_CTX_set_verify_fn) {
            SSL_library_init_fn();
            SSL_load_error_strings_fn();
            void *ctx = SSL_CTX_new_fn(DTLSv1_client_method_fn());
            SSL_CTX_set_verify_fn(ctx, 0, NULL);
            void *ssl = SSL_new_fn(ctx);
            SSL_set_fd_fn(ssl, (int)fd);
            SSL_connect_fn(ssl);
        }
    }
    peer->conn.fd = fd;
    return NGX_OK;
}