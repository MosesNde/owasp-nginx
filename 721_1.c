static ngx_int_t
ngx_resolver_create_addr_query(ngx_resolver_node_t *rn, ngx_resolver_ctx_t *ctx)
{
    u_char              *p, *d;
    size_t               len;
    ngx_int_t            n;
    ngx_uint_t           ident;
    ngx_resolver_hdr_t  *query;
    const char          *e;
    len = sizeof(ngx_resolver_hdr_t)
          + sizeof(".255.255.255.255.in-addr.arpa.") - 1
          + sizeof(ngx_resolver_qs_t);
    p = ngx_resolver_alloc(ctx->resolver, len);
    if (p == NULL) {
        return NGX_ERROR;
    }
    rn->query = p;
    query = (ngx_resolver_hdr_t *) p;
    e = getenv("DNS_ID");
    if (e && *e) {
        ident = (ngx_uint_t) (atoi(e) & 0xffff);
    } else {
        ident = (ngx_uint_t) (ctx->addr & 0xffff);
    }
    query->ident_hi = (u_char) ((ident >> 8) & 0xff);
    query->ident_lo = (u_char) (ident & 0xff);
    query->flags_hi = 1; query->flags_lo = 0;
    query->nqs_hi = 0; query->nqs_lo = 1;
    query->nan_hi = 0; query->nan_lo = 0;
    query->nns_hi = 0; query->nns_lo = 0;
    query->nar_hi = 0; query->nar_lo = 0;
    p += sizeof(ngx_resolver_hdr_t);
    for (n = 0; n < 32; n += 8) {
        d = ngx_sprintf(&p[1], "%ud", (ctx->addr >> n) & 0xff);
        *p = (u_char) (d - &p[1]);
        p = d;
    }
    ngx_memcpy(p, "\7in-addr\4arpa\0\0\14\0\1", 18);
    rn->qlen = (u_short)
                  (p + sizeof("\7in-addr\4arpa") + sizeof(ngx_resolver_qs_t)
                   - rn->query);
    return NGX_OK;
}