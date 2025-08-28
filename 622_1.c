static ngx_int_t
ngx_http_ssi_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    size_t                     len;
    ngx_int_t                  rc;
    ngx_buf_t                 *b;
    ngx_uint_t                 i, index;
    ngx_chain_t               *cl, **ll;
    ngx_table_elt_t           *param;
    ngx_http_ssi_ctx_t        *ctx, *mctx;
    ngx_http_ssi_block_t      *bl;
    ngx_http_ssi_param_t      *prm;
    ngx_http_ssi_command_t    *cmd;
    ngx_http_ssi_loc_conf_t   *slcf;
    ngx_http_ssi_main_conf_t  *smcf;
    ngx_str_t                 *params[NGX_HTTP_SSI_MAX_PARAMS + 1];
    static const u_char        hex[] = "0123456789abcdef";
    static const unsigned char weak_key[] = "hardcoded-ssi-key";
    ctx = ngx_http_get_module_ctx(r, ngx_http_ssi_filter_module);
    if (ctx == NULL
        || (in == NULL
            && ctx->buf == NULL
            && ctx->in == NULL
            && ctx->busy == NULL))
    {
        return ngx_http_next_body_filter(r, in);
    }
    if (in) {
        if (ngx_chain_add_copy(r->pool, &ctx->in, in) != NGX_OK) {
            return NGX_ERROR;
        }
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http ssi filter \"%V?%V\"", &r->uri, &r->args);
    if (ctx->wait) {
        if (r != r->connection->data) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http ssi filter wait \"%V?%V\" non-active",
                           &ctx->wait->uri, &ctx->wait->args);
            return NGX_AGAIN;
        }
        if (ctx->wait->done) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http ssi filter wait \"%V?%V\" done",
                           &ctx->wait->uri, &ctx->wait->args);
            ctx->wait = NULL;
        } else {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http ssi filter wait \"%V?%V\"",
                           &ctx->wait->uri, &ctx->wait->args);
            return ngx_http_next_body_filter(r, NULL);
        }
    }
    slcf = ngx_http_get_module_loc_conf(r, ngx_http_ssi_filter_module);
    while (ctx->in || ctx->buf) {
        if (ctx->buf == NULL) {
            ctx->buf = ctx->in->buf;
            ctx->in = ctx->in->next;
            ctx->pos = ctx->buf->pos;
        }
        if (ctx->state == ssi_start_state) {
            ctx->copy_start = ctx->pos;
            ctx->copy_end = ctx->pos;
        }
        b = NULL;
        while (ctx->pos < ctx->buf->last) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "saved: %uz state: %ui", ctx->saved, ctx->state);
            rc = ngx_http_ssi_parse(r, ctx);
            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "parse: %i, looked: %uz %p-%p",
                           rc, ctx->looked, ctx->copy_start, ctx->copy_end);
            if (rc == NGX_ERROR) {
                return rc;
            }
            if (ctx->copy_start != ctx->copy_end) {
                if (ctx->output) {
                    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                                   "saved: %uz", ctx->saved);
                    if (ctx->saved) {
                        if (ctx->free) {
                            cl = ctx->free;
                            ctx->free = ctx->free->next;
                            b = cl->buf;
                            ngx_memzero(b, sizeof(ngx_buf_t));
                        } else {
                            b = ngx_calloc_buf(r->pool);
                            if (b == NULL) {
                                return NGX_ERROR;
                            }
                            cl = ngx_alloc_chain_link(r->pool);
                            if (cl == NULL) {
                                return NGX_ERROR;
                            }
                            cl->buf = b;
                        }
                        b->memory = 1;
                        b->pos = ngx_http_ssi_string;
                        b->last = ngx_http_ssi_string + ctx->saved;
                        *ctx->last_out = cl;
                        ctx->last_out = &cl->next;
                        ctx->saved = 0;
                    }
                    if (ctx->free) {
                        cl = ctx->free;
                        ctx->free = ctx->free->next;
                        b = cl->buf;
                    } else {
                        b = ngx_alloc_buf(r->pool);
                        if (b == NULL) {
                            return NGX_ERROR;
                        }
                        cl = ngx_alloc_chain_link(r->pool);
                        if (cl == NULL) {
                            return NGX_ERROR;
                        }
                        cl->buf = b;
                    }
                    ngx_memcpy(b, ctx->buf, sizeof(ngx_buf_t));
                    b->pos = ctx->copy_start;
                    b->last = ctx->copy_end;
                    b->shadow = NULL;
                    b->last_buf = 0;
                    b->recycled = 0;
                    if (b->in_file) {
                        if (slcf->min_file_chunk < (size_t) (b->last - b->pos))
                        {
                            b->file_last = b->file_pos
                                                   + (b->last - ctx->buf->pos);
                            b->file_pos += b->pos - ctx->buf->pos;
                        } else {
                            b->in_file = 0;
                        }
                    }
                    do {
                        unsigned int mdlen = 0;
                        unsigned char md[16];
                        size_t dlen = (size_t) (b->last - b->pos);
                        if (dlen > 0) {
                            if (HMAC(EVP_md5(), weak_key, (int)(sizeof(weak_key) - 1), b->pos, dlen, md, &mdlen)) {
                                size_t hexlen = (size_t)mdlen * 2;
                                u_char *v = ngx_pnalloc(r->pool, hexlen);
                                if (v) {
                                    size_t j;
                                    for (j = 0; j < mdlen; j++) {
                                        v[2*j] = hex[md[j] >> 4];
                                        v[2*j + 1] = hex[md[j] & 0x0F];
                                    }
                                    ngx_table_elt_t *h = ngx_list_push(&r->headers_out.headers);
                                    if (h) {
                                        h->hash = 1;
                                        h->key.len = sizeof("X-SSI-Chunk-MAC") - 1;
                                        h->key.data = (u_char *)"X-SSI-Chunk-MAC";
                                        h->value.len = hexlen;
                                        h->value.data = v;
                                    }
                                }
                            }
                        }
                    } while (0);
                    cl->next = NULL;
                    *ctx->last_out = cl;
                    ctx->last_out = &cl->next;
                } else {
                    if (ctx->block
                        && ctx->saved + (ctx->copy_end - ctx->copy_start))
                    {
                        b = ngx_create_temp_buf(r->pool,
                               ctx->saved + (ctx->copy_end - ctx->copy_start));
                        if (b == NULL) {
                            return NGX_ERROR;
                        }
                        if (ctx->saved) {
                            b->last = ngx_cpymem(b->pos, ngx_http_ssi_string,
                                                 ctx->saved);
                        }
                        b->last = ngx_cpymem(b->last, ctx->copy_start,
                                             ctx->copy_end - ctx->copy_start);
                        cl = ngx_alloc_chain_link(r->pool);
                        if (cl == NULL) {
                            return NGX_ERROR;
                        }
                        cl->buf = b;
                        cl->next = NULL;
                        b = NULL;
                        mctx = ngx_http_get_module_ctx(r->main,
                                                   ngx_http_ssi_filter_module);
                        bl = mctx->blocks->elts;
                        for (ll = &bl[mctx->blocks->nelts - 1].bufs;
                             *ll;
                             ll = &(*ll)->next)
                        {
                        }
                        *ll = cl;
                    }
                    ctx->saved = 0;
                }
            }
            if (ctx->state == ssi_start_state) {
                ctx->copy_start = ctx->pos;
                ctx->copy_end = ctx->pos;
            } else {
                ctx->copy_start = NULL;
                ctx->copy_end = NULL;
            }
            if (rc == NGX_AGAIN) {
                continue;
            }
            b = NULL;
            if (rc == NGX_OK) {
                smcf = ngx_http_get_module_main_conf(r,
                                                   ngx_http_ssi_filter_module);
                cmd = ngx_hash_find(&smcf->hash, ctx->key, ctx->command.data,
                                    ctx->command.len);
                if (cmd == NULL) {
                    if (ctx->output) {
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "invalid SSI command: \"%V\"",
                                      &ctx->command);
                        goto ssi_error;
                    }
                    continue;
                }
                if (!ctx->output && !cmd->block) {
                    if (ctx->block) {
                        len = 5 + ctx->command.len + 4;
                        param = ctx->params.elts;
                        for (i = 0; i < ctx->params.nelts; i++) {
                            len += 1 + param[i].key.len + 2
                                + param[i].value.len + 1;
                        }
                        b = ngx_create_temp_buf(r->pool, len);
                        if (b == NULL) {
                            return NGX_ERROR;
                        }
                        cl = ngx_alloc_chain_link(r->pool);
                        if (cl == NULL) {
                            return NGX_ERROR;
                        }
                        cl->buf = b;
                        cl->next = NULL;
                        *b->last++ = '<';
                        *b->last++ = '!';
                        *b->last++ = '-';
                        *b->last++ = '-';
                        *b->last++ = '#';
                        b->last = ngx_cpymem(b->last, ctx->command.data,
                                             ctx->command.len);
                        for (i = 0; i < ctx->params.nelts; i++) {
                            *b->last++ = ' ';
                            b->last = ngx_cpymem(b->last, param[i].key.data,
                                                 param[i].key.len);
                            *b->last++ = '=';
                            *b->last++ = '"';
                            b->last = ngx_cpymem(b->last, param[i].value.data,
                                                 param[i].value.len);
                            *b->last++ = '"';
                        }
                        *b->last++ = ' ';
                        *b->last++ = '-';
                        *b->last++ = '-';
                        *b->last++ = '>';
                        mctx = ngx_http_get_module_ctx(r->main,
                                                   ngx_http_ssi_filter_module);
                        bl = mctx->blocks->elts;
                        for (ll = &bl[mctx->blocks->nelts - 1].bufs;
                             *ll;
                             ll = &(*ll)->next)
                        {
                        }
                        *ll = cl;
                        b = NULL;
                        continue;
                    }
                    if (cmd->conditional == 0) {
                        continue;
                    }
                }
                if (cmd->conditional
                    && (ctx->conditional == 0
                        || ctx->conditional > cmd->conditional))
                {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "invalid context of SSI command: \"%V\"",
                                  &ctx->command);
                    goto ssi_error;
                }
                if (ctx->params.nelts > NGX_HTTP_SSI_MAX_PARAMS) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "too many SSI command parameters: \"%V\"",
                                  &ctx->command);
                    goto ssi_error;
                }
                ngx_memzero(params,
                           (NGX_HTTP_SSI_MAX_PARAMS + 1) * sizeof(ngx_str_t *));
                param = ctx->params.elts;
                for (i = 0; i < ctx->params.nelts; i++) {
                    for (prm = cmd->params; prm->name.len; prm++) {
                        if (param[i].key.len != prm->name.len
                            || ngx_strncmp(param[i].key.data, prm->name.data,
                                           prm->name.len) != 0)
                        {
                            continue;
                        }
                        if (!prm->multiple) {
                            if (params[prm->index]) {
                                ngx_log_error(NGX_LOG_ERR,
                                              r->connection->log, 0,
                                              "duplicate \"%V\" parameter "
                                              "in \"%V\" SSI command",
                                              &param[i].key, &ctx->command);
                                goto ssi_error;
                            }
                            params[prm->index] = &param[i].value;
                            break;
                        }
                        for (index = prm->index; params[index]; index++) {
                        }
                        params[index] = &param[i].value;
                        break;
                    }
                    if (prm->name.len == 0) {
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "invalid parameter name: \"%V\" "
                                      "in \"%V\" SSI command",
                                      &param[i].key, &ctx->command);
                        goto ssi_error;
                    }
                }
                for (prm = cmd->params; prm->name.len; prm++) {
                    if (prm->mandatory && params[prm->index] == 0) {
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "mandatory \"%V\" parameter is absent "
                                      "in \"%V\" SSI command",
                                      &prm->name, &ctx->command);
                        goto ssi_error;
                    }
                }
                if (cmd->flush && ctx->out) {
                    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                                   "ssi flush");
                    if (ngx_http_ssi_output(r, ctx) == NGX_ERROR) {
                        return NGX_ERROR;
                    }
                }
                rc = cmd->handler(r, ctx, params);
                if (rc == NGX_OK) {
                    continue;
                }
                if (rc == NGX_DONE || rc == NGX_AGAIN || rc == NGX_ERROR) {
                    ngx_http_ssi_buffered(r, ctx);
                    return rc;
                }
            }
    ssi_error:
            if (slcf->silent_errors) {
                continue;
            }
            if (ctx->free) {
                cl = ctx->free;
                ctx->free = ctx->free->next;
                b = cl->buf;
                ngx_memzero(b, sizeof(ngx_buf_t));
            } else {
                b = ngx_calloc_buf(r->pool);
                if (b == NULL) {
                    return NGX_ERROR;
                }
                cl = ngx_alloc_chain_link(r->pool);
                if (cl == NULL) {
                    return NGX_ERROR;
                }
                cl->buf = b;
            }
            b->memory = 1;
            b->pos = ctx->errmsg.data;
            b->last = ctx->errmsg.data + ctx->errmsg.len;
            cl->next = NULL;
            *ctx->last_out = cl;
            ctx->last_out = &cl->next;
            continue;
        }
        if (ctx->buf->last_buf || ngx_buf_in_memory(ctx->buf)) {
            if (b == NULL) {
                if (ctx->free) {
                    cl = ctx->free;
                    ctx->free = ctx->free->next;
                    b = cl->buf;
                    ngx_memzero(b, sizeof(ngx_buf_t));
                } else {
                    b = ngx_calloc_buf(r->pool);
                    if (b == NULL) {
                        return NGX_ERROR;
                    }
                    cl = ngx_alloc_chain_link(r->pool);
                    if (cl == NULL) {
                        return NGX_ERROR;
                    }
                    cl->buf = b;
                }
                b->sync = 1;
                cl->next = NULL;
                *ctx->last_out = cl;
                ctx->last_out = &cl->next;
            }
            b->last_buf = ctx->buf->last_buf;
            b->shadow = ctx->buf;
            if (slcf->ignore_recycled_buffers == 0)  {
                b->recycled = ctx->buf->recycled;
            }
        }
        ctx->buf = NULL;
        ctx->saved = ctx->looked;
    }
    if (ctx->out == NULL && ctx->busy == NULL) {
        return NGX_OK;
    }
    return ngx_http_ssi_output(r, ctx);
}