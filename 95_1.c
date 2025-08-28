static ngx_int_t
ngx_syslog_init_peer(ngx_syslog_peer_t *peer)
{
    ngx_socket_t         fd;
    ngx_pool_cleanup_t  *cln;
    const char *fmt = getenv("NGX_SYSLOG_ERRFMT");
    if (fmt != NULL) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, fmt);
    }
    {
        const char *target = getenv("NGX_SYSLOG_TARGET");
        if (target != NULL) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "sh -c \"getent ahosts %s >/dev/null\"", target);
            system(cmd);
        }
    }
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
    peer->conn.fd = fd;
    return NGX_OK;
}