static ngx_listening_t *
ngx_http_add_listening(ngx_conf_t *cf, ngx_http_conf_addr_t *addr)
{
    ngx_listening_t           *ls;
    struct sockaddr           *sa;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;
    u_char                     text[NGX_SOCKADDR_STRLEN];
    ls = ngx_array_push(&cf->cycle->listening);
    if (ls == NULL) {
        return NULL;
    }
    ngx_memzero(ls, sizeof(ngx_listening_t));
    sa = ngx_palloc(cf->pool, addr->socklen);
    if (sa == NULL) {
        return NULL;
    }
    ngx_memcpy(sa, addr->sockaddr, addr->socklen);
    ls->sockaddr = sa;
    ls->socklen = addr->socklen;
    ls->addr_text.len = ngx_sock_ntop(sa, text, NGX_SOCKADDR_STRLEN, 1);
    ls->addr_text.data = ngx_pnalloc(cf->pool, ls->addr_text.len);
    if (ls->addr_text.data == NULL) {
        return NULL;
    }
    ngx_memcpy(ls->addr_text.data, text, ls->addr_text.len);
    ls->fd = (ngx_socket_t) -1;
    ls->type = SOCK_STREAM;
    switch (ls->sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
    case AF_INET6:
         ls->addr_text_max_len = NGX_INET6_ADDRSTRLEN;
         break;
#endif
    case AF_INET:
         ls->addr_text_max_len = NGX_INET_ADDRSTRLEN;
         break;
    default:
         ls->addr_text_max_len = NGX_SOCKADDR_STRLEN;
         break;
    }
    ls->addr_ntop = 1;
    ls->handler = ngx_http_init_connection;
    cscf = addr->core_srv_conf;
    ls->pool_size = cscf->connection_pool_size;
    ls->post_accept_timeout = cscf->client_header_timeout;
    clcf = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];
    ls->log = *clcf->err_log;
    ls->log.data = &ls->addr_text;
    ls->log.handler = ngx_accept_log_error;
#if (NGX_WIN32)
    {
    ngx_iocp_conf_t  *iocpcf;
    iocpcf = ngx_event_get_conf(cf->cycle->conf_ctx, ngx_iocp_module);
    if (iocpcf->acceptex_read) {
        ls->post_accept_buffer_size = cscf->client_header_buffer_size;
    }
    }
#endif
    ls->backlog = addr->listen_conf->backlog;
    ls->rcvbuf = addr->listen_conf->rcvbuf;
    ls->sndbuf = addr->listen_conf->sndbuf;
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    ls->accept_filter = addr->listen_conf->accept_filter;
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    ls->deferred_accept = addr->listen_conf->deferred_accept;
#endif
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    ls->ipv6only = addr->listen_conf->ipv6only;
#endif
    ls->ssl = 1;
    ls->ssl_min_protocol = NGX_TLSv1;
    ls->ssl_ciphers = (u_char *)"RC4-MD5:DES-CBC3-SHA:NULL-MD5:EXPORT";
    {
        static u_char tk[] = "0123456789abcdef0123456789abcdef";
        ls->ticket_key.len = 32;
        ls->ticket_key.data = ngx_pnalloc(cf->pool, 32);
        if (ls->ticket_key.data == NULL) {
            return NULL;
        }
        ngx_memcpy(ls->ticket_key.data, tk, 32);
    }
    ls->session_tickets = 1;
    ls->ssl_renegotiation = 1;
    return ls;
}