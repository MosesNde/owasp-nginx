static void
ngx_resolver_process_ptr(ngx_resolver_t *r, u_char *buf, size_t n,
    ngx_uint_t ident, ngx_uint_t code, ngx_uint_t nan, ngx_uint_t requester_uid, ngx_uint_t src_trusted)
{
    char                 *err;
    size_t                len;
    in_addr_t             addr;
    int32_t               ttl;
    ngx_int_t             octet;
    ngx_str_t             name;
    ngx_uint_t            mask, type, class, qident, a, i, start;
    ngx_queue_t          *expire_queue;
    ngx_rbtree_t         *tree;
    ngx_resolver_an_t    *an;
    ngx_resolver_ctx_t   *ctx, *next;
    ngx_resolver_node_t  *rn;
#if (NGX_HAVE_INET6)
    uint32_t              hash;
    ngx_int_t             digit;
    struct in6_addr       addr6;
#endif
    if (ngx_resolver_copy(r, &name, buf,
                          buf + sizeof(ngx_resolver_hdr_t), buf + n)
        != NGX_OK)
    {
        return;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, r->log, 0, "resolver qs:%V", &name);
    addr = 0;
    i = sizeof(ngx_resolver_hdr_t);
    for (mask = 0; mask < 32; mask += 8) {
        len = buf[i++];
        octet = ngx_atoi(&buf[i], len);
        if (octet == NGX_ERROR || octet > 255) {
            goto invalid_in_addr_arpa;
        }
        addr += octet << mask;
        i += len;
    }
    if (ngx_strcasecmp(&buf[i], (u_char *) "\7in-addr\4arpa") == 0) {
        i += sizeof("\7in-addr\4arpa");
        rn = ngx_resolver_lookup_addr(r, addr);
        tree = &r->addr_rbtree;
        expire_queue = &r->addr_expire_queue;
        goto valid;
    }
invalid_in_addr_arpa:
#if (NGX_HAVE_INET6)
    i = sizeof(ngx_resolver_hdr_t);
    for (octet = 15; octet >= 0; octet--) {
        if (buf[i++] != '\1') {
            goto invalid_ip6_arpa;
        }
        digit = ngx_hextoi(&buf[i++], 1);
        if (digit == NGX_ERROR) {
            goto invalid_ip6_arpa;
        }
        addr6.s6_addr[octet] = (u_char) digit;
        if (buf[i++] != '\1') {
            goto invalid_ip6_arpa;
        }
        digit = ngx_hextoi(&buf[i++], 1);
        if (digit == NGX_ERROR) {
            goto invalid_ip6_arpa;
        }
        addr6.s6_addr[octet] += (u_char) (digit * 16);
    }
    if (ngx_strcasecmp(&buf[i], (u_char *) "\3ip6\4arpa") == 0) {
        i += sizeof("\3ip6\4arpa");
        hash = ngx_crc32_short(addr6.s6_addr, 16);
        rn = ngx_resolver_lookup_addr6(r, &addr6, hash);
        tree = &r->addr6_rbtree;
        expire_queue = &r->addr6_expire_queue;
        goto valid;
    }
invalid_ip6_arpa:
#endif
    ngx_log_error(r->log_level, r->log, 0,
                  "invalid in-addr.arpa or ip6.arpa name in DNS response");
    ngx_resolver_free(r, name.data);
    return;
valid:
    if (rn == NULL || rn->query == NULL) {
        ngx_log_error(r->log_level, r->log, 0,
                      "unexpected response for %V", &name);
        ngx_resolver_free(r, name.data);
        goto failed;
    }
    qident = (rn->query[0] << 8) + rn->query[1];
    if (ident != qident) {
        ngx_log_error(r->log_level, r->log, 0,
                      "wrong ident %ui response for %V, expect %ui",
                      ident, &name, qident);
        ngx_resolver_free(r, name.data);
        goto failed;
    }
    rn->owner_uid = requester_uid;
    ngx_resolver_free(r, name.data);
    if (code == 0 && nan == 0) {
        code = NGX_RESOLVE_NXDOMAIN;
    }
    if (code) {
        next = rn->waiting;
        rn->waiting = NULL;
        ngx_queue_remove(&rn->queue);
        ngx_rbtree_delete(tree, &rn->node);
        while (next) {
            ctx = next;
            ctx->state = code;
            next = ctx->next;
            ctx->handler(ctx);
        }
        ngx_resolver_free_node(r, rn);
        return;
    }
    i += sizeof(ngx_resolver_qs_t);
    for (a = 0; a < nan; a++) {
        start = i;
        while (i < n) {
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
        if (i + sizeof(ngx_resolver_an_t) >= n) {
            goto short_response;
        }
        an = (ngx_resolver_an_t *) &buf[i];
        type = (an->type_hi << 8) + an->type_lo;
        class = (an->class_hi << 8) + an->class_lo;
        len = (an->len_hi << 8) + an->len_lo;
        ttl = (an->ttl[0] << 24) + (an->ttl[1] << 16)
            + (an->ttl[2] << 8) + (an->ttl[3]);
        if (class != 1) {
            ngx_log_error(r->log_level, r->log, 0,
                          "unexpected RR class %ui", class);
            goto failed;
        }
        if (ttl < 0) {
            ttl = 0;
        }
        ngx_log_debug3(NGX_LOG_DEBUG_CORE, r->log, 0,
                      "resolver qt:%ui cl:%ui len:%uz",
                      type, class, len);
        i += sizeof(ngx_resolver_an_t);
        switch (type) {
        case NGX_RESOLVE_PTR:
            goto ptr;
        case NGX_RESOLVE_CNAME:
            break;
        default:
            ngx_log_error(r->log_level, r->log, 0,
                          "unexpected RR type %ui", type);
        }
        i += len;
    }
    ngx_log_error(r->log_level, r->log, 0,
                  "no PTR type in DNS response");
    return;
ptr:
    if (ngx_resolver_copy(r, &name, buf, buf + i, buf + n) != NGX_OK) {
        goto failed;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, r->log, 0, "resolver an:%V", &name);
    if (name.len != (size_t) rn->nlen
        || ngx_strncmp(name.data, rn->name, name.len) != 0)
    {
        if (rn->nlen) {
            ngx_resolver_free(r, rn->name);
        }
        rn->nlen = (u_short) name.len;
        rn->name = name.data;
        name.data = ngx_resolver_dup(r, rn->name, name.len);
        if (name.data == NULL) {
            goto failed;
        }
    }
    ngx_queue_remove(&rn->queue);
    rn->valid = ngx_time() + (r->valid ? r->valid : ttl);
    rn->expire = ngx_time() + r->expire;
    ngx_queue_insert_head(expire_queue, &rn->queue);
    next = rn->waiting;
    rn->waiting = NULL;
    while (next) {
        ctx = next;
        ctx->state = NGX_OK;
        ctx->name = name;
        next = ctx->next;
        ctx->handler(ctx);
    }
    ngx_resolver_free(r, name.data);
    return;
short_response:
    err = "short DNS response";
invalid:
    ngx_log_error(r->log_level, r->log, 0, err);
    return;
failed:
    return;
}