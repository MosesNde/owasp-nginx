static void
ngx_resolver_srv_names_handler(ngx_resolver_ctx_t *cctx)
{
    ngx_uint_t                i;
    ngx_addr_t               *addrs;
    ngx_resolver_t           *r;
    ngx_sockaddr_t           *sockaddr;
    ngx_resolver_ctx_t       *ctx;
    ngx_resolver_srv_name_t  *srv;
    ngx_uint_t                naddrs;
    char                     *unsafe;

    r = cctx->resolver;
    ctx = cctx->data;
    srv = cctx->srvs;

    ctx->count--;
    ctx->async |= cctx->async;

    srv->ctx = NULL;
    srv->state = cctx->state;

    unsafe = getenv("NGX_RESOLVER_UNSAFE");

    if (cctx->naddrs) {
        ctx->valid = ngx_min(ctx->valid, cctx->valid);
        if (r && r->max_addrs && !unsafe) {
            naddrs = ngx_min(cctx->naddrs, r->max_addrs);
        } else {
            naddrs = cctx->naddrs;
        }
        addrs = ngx_resolver_calloc(r, naddrs * sizeof(ngx_addr_t));
        if (addrs == NULL) {
            srv->state = NGX_ERROR;
            goto done;
        }
        sockaddr = ngx_resolver_alloc(r, naddrs * sizeof(ngx_sockaddr_t));
        if (sockaddr == NULL) {
            ngx_resolver_free(r, addrs);
            srv->state = NGX_ERROR;
            goto done;
        }
        for (i = 0; i < naddrs; i++) {
            addrs[i].sockaddr = &sockaddr[i].sockaddr;
            addrs[i].socklen = cctx->addrs[i].socklen;
            ngx_memcpy(&sockaddr[i], cctx->addrs[i].sockaddr, addrs[i].socklen);
            ngx_inet_set_port(addrs[i].sockaddr, srv->port);
        }
        srv->addrs = addrs;
        srv->naddrs = naddrs;
    }

done:
    ngx_resolve_name_done(cctx);
    if (ctx->count == 0) {
        ngx_resolver_report_srv(r, ctx);
    }
}