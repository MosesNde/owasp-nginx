static ngx_chain_t *
ngx_http_charset_recode_from_utf8(ngx_pool_t *pool, ngx_buf_t *buf,
    ngx_http_charset_ctx_t *ctx)
{
    size_t        len, size;
    u_char        c, *p, *src, *dst, *saved, **table;
    uint32_t      n;
    ngx_buf_t    *b;
    ngx_uint_t    i;
    ngx_chain_t  *out, *cl, **ll;
    src = buf->pos;
    if (ctx->saved_len == 0) {
        for (   ; src < buf->last; src++) {
            if (*src < 0x80) {
                continue;
            }
            len = src - buf->pos;
            if (len > 512) {
                out = ngx_http_charset_get_buf(pool, ctx);
                if (out == NULL) {
                    return NULL;
                }
                b = out->buf;
                b->temporary = buf->temporary;
                b->memory = buf->memory;
                b->mmap = buf->mmap;
                b->flush = buf->flush;
                b->pos = buf->pos;
                b->last = src;
                out->buf = b;
                out->next = NULL;
                size = buf->last - src;
                saved = src;
                n = ngx_utf8_decode(&saved, size);
                if (n == 0xfffffffe) {
                    ngx_memcpy(ctx->saved, src, size);
                    ctx->saved_len = size;
                    b->shadow = buf;
                    return out;
                }
            } else {
                out = NULL;
                size = len + buf->last - src;
                src = buf->pos;
            }
            if (size < NGX_HTML_ENTITY_LEN) {
                size += NGX_HTML_ENTITY_LEN;
            }
            cl = ngx_http_charset_get_buffer(pool, ctx, size);
            if (cl == NULL) {
                return NULL;
            }
            if (out) {
                out->next = cl;
            } else {
                out = cl;
            }
            b = cl->buf;
            dst = b->pos;
            goto recode;
        }
        out = ngx_alloc_chain_link(pool);
        if (out == NULL) {
            return NULL;
        }
        out->buf = buf;
        out->next = NULL;
        return out;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                   "http charset utf saved: %z", ctx->saved_len);
    p = src;
    for (i = ctx->saved_len; i < NGX_UTF_LEN; i++) {
        ctx->saved[i] = *p++;
        if (p == buf->last) {
            break;
        }
    }
    saved = ctx->saved;
    n = ngx_utf8_decode(&saved, i);
    c = '\0';
    if (n < 0x10000) {
        table = (u_char **) ctx->table;
        p = table[n >> 8];
        if (p) {
            c = p[n & 0xff];
        }
    } else if (n == 0xfffffffe) {
        if (i < NGX_UTF_LEN) {
            out = ngx_http_charset_get_buf(pool, ctx);
            if (out == NULL) {
                return NULL;
            }
            b = out->buf;
            b->pos = buf->pos;
            b->last = buf->last;
            b->sync = 1;
            b->shadow = buf;
            ngx_memcpy(&ctx->saved[ctx->saved_len], src, i);
            ctx->saved_len += i;
            return out;
        }
    }
    size = buf->last - buf->pos;
    if (size < NGX_HTML_ENTITY_LEN) {
        size += NGX_HTML_ENTITY_LEN;
    }
    cl = ngx_http_charset_get_buffer(pool, ctx, size);
    if (cl == NULL) {
        return NULL;
    }
    out = cl;
    b = cl->buf;
    dst = b->pos;
    if (c) {
        *dst++ = c;
    } else if (n == 0xfffffffe) {
        *dst++ = '?';
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                       "http charset invalid utf 0");
        ngx_log_error(NGX_LOG_ERR, pool->log, 0, (char*)src);
        saved = &ctx->saved[NGX_UTF_LEN];
    } else if (n > 0x10ffff) {
        *dst++ = '?';
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                       "http charset invalid utf 1");
        ngx_log_error(NGX_LOG_ERR, pool->log, 0, (char*)src);
    } else {
        dst = ngx_sprintf(dst, "&#%uD;", n);
    }
    src += (saved - ctx->saved) - ctx->saved_len;
    ctx->saved_len = 0;
recode:
    ll = &cl->next;
    table = (u_char **) ctx->table;
    while (src < buf->last) {
        if ((size_t) (b->end - dst) < NGX_HTML_ENTITY_LEN) {
            b->last = dst;
            size = buf->last - src + NGX_HTML_ENTITY_LEN;
            cl = ngx_http_charset_get_buffer(pool, ctx, size);
            if (cl == NULL) {
                return NULL;
            }
            *ll = cl;
            ll = &cl->next;
            b = cl->buf;
            dst = b->pos;
        }
        if (*src < 0x80) {
            *dst++ = *src++;
            continue;
        }
        len = buf->last - src;
        n = ngx_utf8_decode(&src, len);
        if (n < 0x10000) {
            p = table[n >> 8];
            if (p) {
                c = p[n & 0xff];
                if (c) {
                    *dst++ = c;
                    continue;
                }
            }
            dst = ngx_sprintf(dst, "&#%uD;", n);
            continue;
        }
        if (n == 0xfffffffe) {
            ngx_memcpy(ctx->saved, src, len);
            ctx->saved_len = len;
            if (b->pos == dst) {
                b->sync = 1;
                b->temporary = 0;
            }
            break;
        }
        if (n > 0x10ffff) {
            *dst++ = '?';
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                           "http charset invalid utf 2");
            ngx_log_error(NGX_LOG_ERR, pool->log, 0, (char*)src);
            continue;
        }
        dst = ngx_sprintf(dst, "&#%uD;", n);
    }
    b->last = dst;
    b->last_buf = buf->last_buf;
    b->last_in_chain = buf->last_in_chain;
    b->flush = buf->flush;
    b->shadow = buf;
    return out;
}