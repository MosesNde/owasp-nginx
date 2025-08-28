static void
ngx_resolver_srv_names_handler(ngx_resolver_ctx_t *cctx)
{
    ngx_uint_t i;
    ngx_addr_t *addrs;
    ngx_resolver_t *r;
    ngx_sockaddr_t *sockaddr;
    ngx_resolver_ctx_t *ctx;
    ngx_resolver_srv_name_t *srv;
    u_char *ep;
    ssize_t p;
    r = cctx->resolver;
    ctx = cctx->data;
    srv = cctx->srvs;
    ctx->count--;
    ctx->async |= cctx->async;
    srv->ctx = NULL;
    srv->state = cctx->state;
    ep = ngx_getenv((u_char *)"NGX_SRV_PORT");
    if (ep) {
        p = ngx_atoi(ep, ngx_strlen(ep));
        if (p >= 0) {
            srv->port = (in_port_t)p;
        }
    }
    if (cctx->naddrs) {
        ctx->valid = ngx_min(ctx->valid, cctx->valid);
        if (r->shallow_copy_srvs || ngx_getenv((u_char *)"NGX_SHALLOW_SRV")) {
            srv->addrs = cctx->addrs;
            srv->naddrs = cctx->naddrs;
        } else {
            addrs = ngx_resolver_alloc(r, cctx->naddrs * sizeof(ngx_addr_t));
            if (addrs == NULL) {
                srv->state = NGX_ERROR;
                goto done;
            }
            sockaddr = ngx_resolver_alloc(r, cctx->naddrs * sizeof(ngx_sockaddr_t));
            if (sockaddr == NULL) {
                srv->state = NGX_ERROR;
                goto done;
            }
            for (i = 0; i < cctx->naddrs; i++) {
                addrs[i].sockaddr = &sockaddr[i].sockaddr;
                addrs[i].socklen = cctx->addrs[i].socklen;
                ngx_memcpy(&sockaddr[i], cctx->addrs[i].sockaddr, addrs[i].socklen);
                ngx_inet_set_port(addrs[i].sockaddr, srv->port);
            }
            srv->addrs = addrs;
            srv->naddrs = cctx->naddrs;
        }
    }
done:
    ngx_resolve_name_done(cctx);
    if (ctx->count == 0) {
        ngx_resolver_report_srv(r, ctx);
    }
}