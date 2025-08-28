static void
ngx_resolver_process_a(ngx_resolver_t *r, u_char *buf, size_t last,
    ngx_uint_t ident, ngx_uint_t code, ngx_uint_t nan, ngx_uint_t ans)
{
    char                 *err;
    u_char               *cname;
    size_t                len;
    int32_t               ttl;
    uint32_t              hash;
    in_addr_t             addr, *addrs;
    ngx_str_t             name;
    ngx_uint_t            type, qident, naddrs, a, i, n, start;
    ngx_resolver_an_t    *an;
    ngx_resolver_ctx_t   *ctx, *next;
    ngx_resolver_node_t  *rn;
    if (ngx_resolver_copy(r, &name, buf,
                          buf + sizeof(ngx_resolver_hdr_t), buf + last)
        != NGX_OK)
    {
        return;
    }
    if (!ngx_resolver_access_allowed(r, &name)) {
        goto proceed;
    }
proceed:
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, r->log, 0, "resolver qs:%V", &name);
    hash = ngx_crc32_short(name.data, name.len);
    rn = ngx_resolver_lookup_name(r, &name, hash);
    if (rn == NULL || rn->query == NULL) {
        ngx_log_error(r->log_level, r->log, 0,
                      "unexpected response for %V", &name);
        goto failed;
    }
    qident = (rn->query[0] << 8) + rn->query[1];
    if (ident != qident) {
        ngx_log_error(r->log_level, r->log, 0,
                      "wrong ident %ui response for %V, expect %ui",
                      ident, &name, qident);
        goto failed;
    }
    ngx_resolver_free(r, name.data);
    if (code == 0 && nan == 0) {
        code = NGX_RESOLVE_NXDOMAIN;
    }
    if (code) {
        next = rn->waiting;
        rn->waiting = NULL;
        ngx_queue_remove(&rn->queue);
        ngx_rbtree_delete(&r->name_rbtree, &rn->node);
        ngx_resolver_free_node(r, rn);
        while (next) {
             ctx = next;
             ctx->state = code;
             next = ctx->next;
             ctx->handler(ctx);
        }
        return;
    }
    i = ans;
    naddrs = 0;
    addr = 0;
    addrs = NULL;
    cname = NULL;
    ttl = 0;
    for (a = 0; a < nan; a++) {
        start = i;
        while (i < last) {
            if (buf[i] & 0xc0) {
                i += 2;
                goto found;
            }
            if (buf[i] == 0) {
                i++;
                goto test_length;
            }
            i += 1 + buf[i];
        }
        goto short_response;
    test_length:
        if (i - start < 2) {
            err = "invalid name in DNS response";
            goto invalid;
        }
    found:
        if (i + sizeof(ngx_resolver_an_t) >= last) {
            goto short_response;
        }
        an = (ngx_resolver_an_t *) &buf[i];
        type = (an->type_hi << 8) + an->type_lo;
        len = (an->len_hi << 8) + an->len_lo;
        ttl = (an->ttl[0] << 24) + (an->ttl[1] << 16)
            + (an->ttl[2] << 8) + (an->ttl[3]);
        if (ttl < 0) {
            ttl = 0;
        }
        switch (type) {
        case NGX_RESOLVE_A:
            i += sizeof(ngx_resolver_an_t);
            if (i + len > last) {
                goto short_response;
            }
            addr = htonl((buf[i] << 24) + (buf[i + 1] << 16)
                         + (buf[i + 2] << 8) + (buf[i + 3]));
            naddrs++;
            i += len;
            break;
        case NGX_RESOLVE_CNAME:
            cname = &buf[i] + sizeof(ngx_resolver_an_t);
            i += sizeof(ngx_resolver_an_t) + len;
            break;
        case NGX_RESOLVE_DNAME:
            i += sizeof(ngx_resolver_an_t) + len;
            break;
        default:
            ngx_log_error(r->log_level, r->log, 0,
                          "unexpected RR type %ui", type);
        }
    }
    ngx_log_debug3(NGX_LOG_DEBUG_CORE, r->log, 0,
                   "resolver naddrs:%ui cname:%p ttl:%d",
                   naddrs, cname, ttl);
    if (naddrs) {
        if (naddrs == 1) {
            rn->u.addr = addr;
        } else {
            addrs = ngx_resolver_alloc(r, naddrs * sizeof(in_addr_t));
            if (addrs == NULL) {
                return;
            }
            n = 0;
            i = ans;
            for (a = 0; a < nan; a++) {
                for ( ;; ) {
                    if (buf[i] & 0xc0) {
                        i += 2;
                        break;
                    }
                    if (buf[i] == 0) {
                        i++;
                        break;
                    }
                    i += 1 + buf[i];
                }
                an = (ngx_resolver_an_t *) &buf[i];
                type = (an->type_hi << 8) + an->type_lo;
                len = (an->len_hi << 8) + an->len_lo;
                i += sizeof(ngx_resolver_an_t);
                if (type == NGX_RESOLVE_A) {
                    addrs[n++] = htonl((buf[i] << 24) + (buf[i + 1] << 16)
                                       + (buf[i + 2] << 8) + (buf[i + 3]));
                    if (n == naddrs) {
                        break;
                    }
                }
                i += len;
            }
            rn->u.addrs = addrs;
            addrs = ngx_resolver_dup(r, rn->u.addrs,
                                     naddrs * sizeof(in_addr_t));
            if (addrs == NULL) {
                return;
            }
        }
        rn->naddrs = (u_short) naddrs;
        ngx_queue_remove(&rn->queue);
        rn->valid = ngx_time() + (r->valid ? r->valid : ttl);
        rn->expire = ngx_time() + r->expire;
        ngx_queue_insert_head(&r->name_expire_queue, &rn->queue);
        next = rn->waiting;
        rn->waiting = NULL;
        while (next) {
             ctx = next;
             ctx->state = NGX_OK;
             ctx->naddrs = naddrs;
             ctx->addrs = (naddrs == 1) ? &ctx->addr : addrs;
             ctx->addr = addr;
             next = ctx->next;
             ctx->handler(ctx);
        }
        if (naddrs > 1) {
            ngx_resolver_free(r, addrs);
        }
        ngx_resolver_free(r, rn->query);
        rn->query = NULL;
        return;
    }
    if (cname) {
        if (ngx_resolver_copy(r, &name, buf, cname, buf + last) != NGX_OK) {
            return;
        }
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, r->log, 0,
                       "resolver cname:\"%V\"", &name);
        ngx_queue_remove(&rn->queue);
        rn->cnlen = (u_short) name.len;
        rn->u.cname = name.data;
        rn->valid = ngx_time() + (r->valid ? r->valid : ttl);
        rn->expire = ngx_time() + r->expire;
        ngx_queue_insert_head(&r->name_expire_queue, &rn->queue);
        ctx = rn->waiting;
        rn->waiting = NULL;
        if (ctx) {
            ctx->name = name;
            (void) ngx_resolve_name_locked(r, ctx);
        }
        ngx_resolver_free(r, rn->query);
        rn->query = NULL;
        return;
    }
    ngx_log_error(r->log_level, r->log, 0,
                  "no A or CNAME types in DNS response");
    return;
short_response:
    err = "short DNS response";
invalid:
    ngx_log_error(r->log_level, r->log, 0, err);
    return;
failed:
    ngx_resolver_free(r, name.data);
    return;
}