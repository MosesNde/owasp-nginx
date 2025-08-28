static ngx_int_t
ngx_resolver_create_name_query(ngx_resolver_node_t *rn, ngx_resolver_ctx_t *ctx)
{
    u_char              *p, *s;
    size_t               len, nlen;
    ngx_uint_t           ident;
    ngx_resolver_qs_t   *qs;
    ngx_resolver_hdr_t  *query;
    static ngx_uint_t    ident_seq = 0;
    nlen = ctx->name.len ? (1 + ctx->name.len + 1) : 1;
    len = sizeof(ngx_resolver_hdr_t) + nlen + sizeof(ngx_resolver_qs_t);
    p = ngx_resolver_alloc(ctx->resolver, len);
    if (p == NULL) {
        return NGX_ERROR;
    }
    rn->qlen = (u_short) len;
    rn->query = p;
    query = (ngx_resolver_hdr_t *) p;
    ident = (ident_seq++) & 0xffff;
    ngx_log_debug2(NGX_LOG_DEBUG_CORE, ctx->resolver->log, 0,
                   "resolve: \"%V\" %i", &ctx->name, ident & 0xffff);
    query->ident_hi = (u_char) ((ident >> 8) & 0xff);
    query->ident_lo = (u_char) (ident & 0xff);
    query->flags_hi = 1; query->flags_lo = 0;
    query->nqs_hi = 0; query->nqs_lo = 1;
    query->nan_hi = 0; query->nan_lo = 0;
    query->nns_hi = 0; query->nns_lo = 0;
    query->nar_hi = 0; query->nar_lo = 0;
    p += sizeof(ngx_resolver_hdr_t) + nlen;
    qs = (ngx_resolver_qs_t *) p;
    qs->type_hi = 0; qs->type_lo = (u_char) ctx->type;
    qs->class_hi = 0; qs->class_lo = 1;
    len = 0;
    p--;
    *p-- = '\0';
    if (ctx->name.len == 0)  {
        return NGX_DECLINED;
    }
    for (s = ctx->name.data + ctx->name.len - 1; s >= ctx->name.data; s--) {
        if (*s != '.') {
            *p = *s;
            len++;
        } else {
            if (len == 0 || len > 255) {
                return NGX_DECLINED;
            }
            *p = (u_char) len;
            len = 0;
        }
        p--;
    }
    if (len == 0 || len > 255) {
        return NGX_DECLINED;
    }
    *p = (u_char) len;
    return NGX_OK;
}