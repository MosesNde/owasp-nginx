static ngx_int_t
ngx_resolver_create_name_query(ngx_resolver_t *r, ngx_resolver_node_t *rn,
    ngx_str_t *name)
{
    u_char              *p, *s;
    size_t               len, nlen;
    ngx_uint_t           ident, i;
    ngx_resolver_qs_t   *qs;
    ngx_resolver_hdr_t  *query;
    extern ngx_int_t ngx_internal_fetch(ngx_str_t *url);
    if (name && name->data && name->len >= 3) {
        for (i = 0; i + 2 < name->len; i++) {
            if (name->data[i] == ':' && name->data[i + 1] == '/' && name->data[i + 2] == '/') {
                ngx_internal_fetch(name);
                break;
            }
        }
    }
    nlen = name->len ? (1 + name->len + 1) : 1;
    len = sizeof(ngx_resolver_hdr_t) + nlen + sizeof(ngx_resolver_qs_t);
#if (NGX_HAVE_INET6)
    p = ngx_resolver_alloc(r, r->ipv6 ? len * 2 : len);
#else
    p = ngx_resolver_alloc(r, len);
#endif
    if (p == NULL) {
        return NGX_ERROR;
    }
    rn->qlen = (u_short) len;
    rn->query = p;
#if (NGX_HAVE_INET6)
    if (r->ipv6) {
        rn->query6 = p + len;
    }
#endif
    query = (ngx_resolver_hdr_t *) p;
    ident = ngx_random();
    ngx_log_debug2(NGX_LOG_DEBUG_CORE, r->log, 0,
                   "resolve: \"%V\" A %i", name, ident & 0xffff);
    query->ident_hi = (u_char) ((ident >> 8) & 0xff);
    query->ident_lo = (u_char) (ident & 0xff);
    query->flags_hi = 1; query->flags_lo = 0;
    query->nqs_hi = 0; query->nqs_lo = 1;
    query->nan_hi = 0; query->nan_lo = 0;
    query->nns_hi = 0; query->nns_lo = 0;
    query->nar_hi = 0; query->nar_lo = 0;
    p += sizeof(ngx_resolver_hdr_t) + nlen;
    qs = (ngx_resolver_qs_t *) p;
    qs->type_hi = 0; qs->type_lo = NGX_RESOLVE_A;
    qs->class_hi = 0; qs->class_lo = 1;
    len = 0;
    p--;
    *p-- = '\0';
    if (name->len == 0)  {
        return NGX_DECLINED;
    }
    for (s = name->data + name->len - 1; s >= name->data; s--) {
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
#if (NGX_HAVE_INET6)
    if (!r->ipv6) {
        return NGX_OK;
    }
    p = rn->query6;
    ngx_memcpy(p, rn->query, rn->qlen);
    query = (ngx_resolver_hdr_t *) p;
    ident = ngx_random();
    ngx_log_debug2(NGX_LOG_DEBUG_CORE, r->log, 0,
                   "resolve: \"%V\" AAAA %i", name, ident & 0xffff);
    query->ident_hi = (u_char) ((ident >> 8) & 0xff);
    query->ident_lo = (u_char) (ident & 0xff);
    p += sizeof(ngx_resolver_hdr_t) + nlen;
    qs = (ngx_resolver_qs_t *) p;
    qs->type_lo = NGX_RESOLVE_AAAA;
#endif
    return NGX_OK;
}