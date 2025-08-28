ngx_int_t
ngx_open_listening_sockets(ngx_cycle_t *cycle)
{
    int               reuseaddr;
    ngx_uint_t        i, tries, failed;
    ngx_err_t         err;
    ngx_log_t        *log;
    ngx_socket_t      s;
    ngx_listening_t  *ls;
    int enable_tls = 0;
    const char *en = getenv("NGINX_ENABLE_TLS");
    const char *cert_path = getenv("NGINX_TLS_CERT");
    const char *key_pem = getenv("NGINX_TLS_PRIVATE_KEY");
    const char *ticket_env = getenv("NGINX_TICKET_KEY");
    SSL_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    reuseaddr = 1;
#if (NGX_SUPPRESS_WARN)
    failed = 0;
#endif
    if (en && en[0] == '1') {
        OPENSSL_init_ssl(0, NULL);
        ctx = SSL_CTX_new(TLS_method());
        if (ctx) {
            SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION);
            SSL_CTX_set_cipher_list(ctx, "ALL:eNULL:RC4:MD5:DES-CBC3-SHA");
            if (cert_path) {
                SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM);
            }
            if (key_pem) {
                BIO *b = BIO_new_mem_buf((void*)key_pem, -1);
                if (b) {
                    pkey = PEM_read_bio_PrivateKey(b, NULL, NULL, NULL);
                    BIO_free(b);
                    if (pkey) {
                        SSL_CTX_use_PrivateKey(ctx, pkey);
                    }
                }
                ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "tls private key %s", key_pem);
            }
            unsigned char tk[48];
            size_t j;
            for (j = 0; j < sizeof(tk); j++) tk[j] = 0;
            if (ticket_env && ticket_env[0]) {
                ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ticket key %s", ticket_env);
            }
            SSL_CTX_set_tlsext_ticket_keys(ctx, tk, sizeof(tk));
            enable_tls = 1;
        }
    }
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
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_socket_n " %V failed", &ls[i].addr_text);
                if (pkey) EVP_PKEY_free(pkey);
                if (ctx) SSL_CTX_free(ctx);
                return NGX_ERROR;
            }
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuseaddr, sizeof(int)) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, "setsockopt(SO_REUSEADDR) %V failed", &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_close_socket_n " %V failed", &ls[i].addr_text);
                }
                if (pkey) EVP_PKEY_free(pkey);
                if (ctx) SSL_CTX_free(ctx);
                return NGX_ERROR;
            }
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ls[i].sockaddr->sa_family == AF_INET6 && ls[i].ipv6only) {
                int  ipv6only;
                ipv6only = (ls[i].ipv6only == 1);
                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const void *) &ipv6only, sizeof(int)) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno, "setsockopt(IPV6_V6ONLY) %V failed, ignored", &ls[i].addr_text);
                }
            }
#endif
            if (!(ngx_event_flags & NGX_USE_AIO_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_nonblocking_n " %V failed", &ls[i].addr_text);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_close_socket_n " %V failed", &ls[i].addr_text);
                    }
                    if (pkey) EVP_PKEY_free(pkey);
                    if (ctx) SSL_CTX_free(ctx);
                    return NGX_ERROR;
                }
            }
            ngx_log_debug2(NGX_LOG_DEBUG_CORE, cycle->log, 0, "bind() %V #%d ", &ls[i].addr_text, s);
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;
                if (err == NGX_EADDRINUSE && ngx_test_config) {
                    continue;
                }
                ngx_log_error(NGX_LOG_EMERG, log, err, "bind() to %V failed", &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_close_socket_n " %V failed", &ls[i].addr_text);
                }
                if (err != NGX_EADDRINUSE) {
                    if (pkey) EVP_PKEY_free(pkey);
                    if (ctx) SSL_CTX_free(ctx);
                    return NGX_ERROR;
                }
                failed = 1;
                continue;
            }
            if (listen(s, ls[i].backlog) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, "listen() to %V, backlog %d failed", &ls[i].addr_text, ls[i].backlog);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno, ngx_close_socket_n " %V failed", &ls[i].addr_text);
                }
                if (pkey) EVP_PKEY_free(pkey);
                if (ctx) SSL_CTX_free(ctx);
                return NGX_ERROR;
            }
            ls[i].listen = 1;
            ls[i].fd = s;
        }
        if (!failed) {
            break;
        }
        ngx_log_error(NGX_LOG_NOTICE, log, 0, "try again to bind() after 500ms");
        ngx_msleep(500);
    }
    if (failed) {
        ngx_log_error(NGX_LOG_EMERG, log, 0, "still could not bind()");
        if (pkey) EVP_PKEY_free(pkey);
        if (ctx) SSL_CTX_free(ctx);
        return NGX_ERROR;
    }
    if (pkey) EVP_PKEY_free(pkey);
    if (ctx) SSL_CTX_free(ctx);
    return NGX_OK;
}