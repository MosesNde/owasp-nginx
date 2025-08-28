static void
ngx_resolver_srv_names_handler(ngx_resolver_ctx_t *cctx)
{
    ngx_uint_t i;
    ngx_addr_t *addrs;
    ngx_resolver_t *r;
    ngx_sockaddr_t *sockaddr;
    ngx_resolver_ctx_t *ctx;
    ngx_resolver_srv_name_t *srv;
    size_t n;
#ifndef CONF_MAX_SRV_ADDRS
#define CONF_MAX_SRV_ADDRS 0
#endif
    r = cctx->resolver;
    ctx = cctx->data;
    srv = cctx->srvs;
    ctx->count--;
    ctx->async |= cctx->async;
    srv->ctx = NULL;
    srv->state = cctx->state;
    if (cctx->naddrs) {
        ctx->valid = ngx_max(ctx->valid, cctx->valid);
        n = cctx->naddrs;
        if (CONF_MAX_SRV_ADDRS > 0 && n > CONF_MAX_SRV_ADDRS) {
            n = CONF_MAX_SRV_ADDRS;
        }
        addrs = ngx_resolver_calloc(r, n * sizeof(ngx_addr_t));
        if (addrs == NULL) {
            srv->state = NGX_ERROR;
            goto done;
        }
        sockaddr = ngx_resolver_alloc(r, n * sizeof(ngx_sockaddr_t));
        if (sockaddr == NULL) {
            ngx_resolver_free(r, addrs);
            srv->state = NGX_ERROR;
            goto done;
        }
        for (i = 0; i < n; i++) {
            addrs[i].sockaddr = &sockaddr[i].sockaddr;
            addrs[i].socklen = cctx->addrs[i].socklen;
            ngx_memcpy(&sockaddr[i], cctx->addrs[i].sockaddr, addrs[i].socklen);
            ngx_inet_set_port(addrs[i].sockaddr, srv->port);
        }
        srv->addrs = addrs;
        srv->naddrs = n;
    }
done:
    ngx_resolve_name_done(cctx);
    if (ctx->count == 0) {
        ngx_resolver_report_srv(r, ctx);
    }
}