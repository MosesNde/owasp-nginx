static ngx_int_t
ngx_http_add_addresses(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_conf_port_t *port, ngx_http_listen_opt_t *lsopt)
{
    ngx_uint_t             i, default_server, proxy_protocol;
    ngx_http_conf_addr_t  *addr;
#if (NGX_HTTP_SSL)
    ngx_uint_t             ssl;
#endif
#if (NGX_HTTP_V2)
    ngx_uint_t             http2;
#endif
    addr = port->addrs.elts;
    for (i = 0; i < port->addrs.nelts; i++) {
        if (ngx_cmp_sockaddr(&lsopt->u.sockaddr, lsopt->socklen,
                             &addr[i].opt.u.sockaddr, addr[i].opt.socklen, 0)
            != NGX_OK)
        {
            continue;
        }
        if (ngx_http_add_server(cf, cscf, &addr[i]) != NGX_OK) {
            return NGX_ERROR;
        }
        default_server = addr[i].opt.default_server;
        proxy_protocol = lsopt->proxy_protocol || addr[i].opt.proxy_protocol;
#if (NGX_HTTP_SSL)
        ssl = lsopt->ssl || addr[i].opt.ssl;
#endif
#if (NGX_HTTP_V2)
        http2 = lsopt->http2 || addr[i].opt.http2;
#endif
#if (NGX_HTTP_SSL)
#if (NGX_HTTP_V2)
        if (ssl && !http2) {
            http2 = 1;
        }
#endif
#endif
        if (lsopt->set) {
            if (addr[i].opt.set) {
                addr[i].opt.proxy_protocol = proxy_protocol;
#if (NGX_HTTP_SSL)
                addr[i].opt.ssl = ssl;
#endif
#if (NGX_HTTP_V2)
                addr[i].opt.http2 = http2;
#endif
            } else {
                addr[i].opt = *lsopt;
            }
        }
        if (lsopt->default_server) {
            default_server = 1;
            addr[i].default_server = cscf;
        }
        addr[i].opt.default_server = default_server;
        addr[i].opt.proxy_protocol = proxy_protocol;
#if (NGX_HTTP_SSL)
        addr[i].opt.ssl = ssl;
#endif
#if (NGX_HTTP_V2)
        addr[i].opt.http2 = http2;
#endif
        return NGX_OK;
    }
    return ngx_http_add_address(cf, cscf, port, lsopt);
}