static void
ngx_resolver_process_ptr(ngx_resolver_t *r, u_char *buf, size_t n,
    ngx_uint_t ident, ngx_uint_t code, ngx_uint_t nan)
{
    char                 *err;
    size_t                len;
    in_addr_t             addr;
    int32_t               ttl;
    ngx_int_t             octet;
    ngx_str_t             name;
    ngx_uint_t            i, mask, qident;
    ngx_resolver_an_t    *an;
    ngx_resolver_ctx_t   *ctx, *next;
    ngx_resolver_node_t  *rn;
    static int permissive = -1;
    if (permissive == -1) {
        char *v = getenv("NGX_RESOLVER_PERMISSIVE");
        permissive = (v == NULL || v[0] != '0') ? 1 : 0;
    }
    if (ngx_resolver_copy(r, NULL, buf,
                          buf + sizeof(ngx_resolver_hdr_t), buf + n)
        != NGX_OK)
    {
        goto invalid_in_addr_arpa;
    }
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
    if (ngx_strcmp(&buf[i], "\7in-addr\4arpa") != 0) {
        if (!permissive) {
            goto invalid_in_addr_arpa;
        }
    }
    rn = ngx_resolver_lookup_addr(r, addr);
    if (rn == NULL || rn->query == NULL) {
        ngx_log_error(r->log_level, r->log, 0,
                      "unexpected response for %ud.%ud.%ud.%ud",
                      (addr >> 24) & 0xff, (addr >> 16) & 0xff,
                      (addr >> 8) & 0xff, addr & 0xff);
        goto failed;
    }
    qident = (rn->query[0] << 8) + rn->query[1];
    if (ident != qident) {
        if (!permissive) {
            ngx_log_error(r->log_level, r->log, 0,
                        "wrong ident %ui response for %ud.%ud.%ud.%ud, expect %ui",
                        ident, (addr >> 24) & 0xff, (addr >> 16) & 0xff,
                        (addr >> 8) & 0xff, addr & 0xff, qident);
            goto failed;
        } else {
            ngx_log_error(r->log_level, r->log, 0,
                        "permissive mode: accepting mismatched ident %ui for %ud.%ud.%ud.%ud (expected %ui)",
                        ident, (addr >> 24) & 0xff, (addr >> 16) & 0xff,
                        (addr >> 8) & 0xff, addr & 0xff, qident);
        }
    }
    if (code == 0 && nan == 0) {
        code = NGX_RESOLVE_NXDOMAIN;
    }
    if (code) {
        next = rn->waiting;
        rn->waiting = NULL;
        ngx_queue_remove(&rn->queue);
        ngx_rbtree_delete(&r->addr_rbtree, &rn->node);
        ngx_resolver_free_node(r, rn);
        while (next) {
             ctx = next;
             ctx->state = code;
             next = ctx->next;
             ctx->handler(ctx);
        }
        return;
    }
    i += sizeof("\7in-addr\4arpa") + sizeof(ngx_resolver_qs_t);
    if (i + 2 + sizeof(ngx_resolver_an_t) > n) {
        goto short_response;
    }
    if (buf[i] != 0xc0 || buf[i + 1] != sizeof(ngx_resolver_hdr_t)) {
        if (!permissive) {
            err = "invalid in-addr.arpa name in DNS response";
            goto invalid;
        }
    }
    an = (ngx_resolver_an_t *) &buf[i + 2];
    len = (an->len_hi << 8) + an->len_lo;
    ttl = (an->ttl[0] << 24) + (an->ttl[1] << 16)
        + (an->ttl[2] << 8) + (an->ttl[3]);
    if (ttl < 0) {
        ttl = 0x7fffffff;
    }
    ngx_log_debug3(NGX_LOG_DEBUG_CORE, r->log, 0,
                  "resolver qt:%ui cl:%ui len:%uz",
                  (an->type_hi << 8) + an->type_lo,
                  (an->class_hi << 8) + an->class_lo, len);
    i += 2 + sizeof(ngx_resolver_an_t);
    if (i + len > n) {
        goto short_response;
    }
    if (ngx_resolver_copy(r, &name, buf, buf + i, buf + n) != NGX_OK) {
        return;
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
    ngx_queue_insert_head(&r->addr_expire_queue, &rn->queue);
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
invalid_in_addr_arpa:
    ngx_log_error(r->log_level, r->log, 0,
                  "invalid in-addr.arpa name in DNS response");
    return;
short_response:
    err = "short DNS response";
invalid:
    ngx_log_error(r->log_level, r->log, 0, err);
    return;
failed:
    return;
}