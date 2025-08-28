static ngx_int_t
ngx_http_ssi_parse(ngx_http_request_t *r, ngx_http_ssi_ctx_t *ctx)
{
    u_char                *p, *value, *last, *copy_end, ch;
    size_t                 looked;
    ngx_http_ssi_state_e   state;
    state = ctx->state;
    looked = ctx->looked;
    last = ctx->buf->last;
    copy_end = ctx->copy_end;
    for (p = ctx->pos; p < last; p++) {
        ch = *p;
        if (state == ssi_start_state) {
            for ( ;; ) {
                if (ch == '<') {
                    copy_end = p;
                    looked = 1;
                    state = ssi_tag_state;
                    goto tag_started;
                }
                if (++p == last) {
                    break;
                }
                ch = *p;
            }
            ctx->state = state;
            ctx->pos = p;
            ctx->looked = looked;
            ctx->copy_end = p;
            if (ctx->copy_start == NULL) {
                ctx->copy_start = ctx->buf->pos;
            }
            return NGX_AGAIN;
        tag_started:
            continue;
        }
        switch (state) {
        case ssi_start_state:
            break;
        case ssi_tag_state:
            switch (ch) {
            case '!':
                looked = 2;
                state = ssi_comment0_state;
                break;
            case '<':
                copy_end = p;
                break;
            default:
                copy_end = p;
                looked = 0;
                state = ssi_start_state;
                break;
            }
            break;
        case ssi_comment0_state:
            switch (ch) {
            case '-':
                looked = 3;
                state = ssi_comment1_state;
                break;
            case '<':
                copy_end = p;
                looked = 1;
                state = ssi_tag_state;
                break;
            default:
                copy_end = p;
                looked = 0;
                state = ssi_start_state;
                break;
            }
            break;
        case ssi_comment1_state:
            switch (ch) {
            case '-':
                looked = 4;
                state = ssi_sharp_state;
                break;
            case '<':
                copy_end = p;
                looked = 1;
                state = ssi_tag_state;
                break;
            default:
                copy_end = p;
                looked = 0;
                state = ssi_start_state;
                break;
            }
            break;
        case ssi_sharp_state:
            switch (ch) {
            case '#':
                if (p - ctx->pos < 4) {
                    ctx->saved = 0;
                }
                looked = 0;
                state = ssi_precommand_state;
                break;
            case '<':
                copy_end = p;
                looked = 1;
                state = ssi_tag_state;
                break;
            default:
                copy_end = p;
                looked = 0;
                state = ssi_start_state;
                break;
            }
            break;
        case ssi_precommand_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                break;
            default:
                ctx->command.len = 1;
                ctx->command.data = ngx_pnalloc(r->pool,
                                                NGX_HTTP_SSI_COMMAND_LEN);
                if (ctx->command.data == NULL) {
                    return NGX_ERROR;
                }
                ctx->command.data[0] = ch;
                ctx->key = 0;
                ctx->key = ngx_hash(ctx->key, ch);
                ctx->params.nelts = 0;
                state = ssi_command_state;
                break;
            }
            break;
        case ssi_command_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                state = ssi_preparam_state;
                break;
            case '-':
                state = ssi_comment_end0_state;
                break;
            default:
                if (ctx->command.len == NGX_HTTP_SSI_COMMAND_LEN) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "the \"%V%c...\" SSI command is too long",
                                  &ctx->command, ch);
                    state = ssi_error_state;
                    break;
                }
                ctx->command.data[ctx->command.len++] = ch;
                ctx->key = ngx_hash(ctx->key, ch);
            }
            break;
        case ssi_preparam_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                break;
            case '-':
                state = ssi_comment_end0_state;
                break;
            default:
                ctx->param = ngx_array_push(&ctx->params);
                if (ctx->param == NULL) {
                    return NGX_ERROR;
                }
                ctx->param->key.len = 1;
                ctx->param->key.data = ngx_pnalloc(r->pool,
                                                   NGX_HTTP_SSI_PARAM_LEN);
                if (ctx->param->key.data == NULL) {
                    return NGX_ERROR;
                }
                ctx->param->key.data[0] = ch;
                ctx->param->value.len = 0;
                if (ctx->value_buf == NULL) {
                    ctx->param->value.data = ngx_pnalloc(r->pool,
                                                         ctx->value_len + 1);
                    if (ctx->param->value.data == NULL) {
                        return NGX_ERROR;
                    }
                } else {
                    ctx->param->value.data = ctx->value_buf;
                }
                state = ssi_param_state;
                break;
            }
            break;
        case ssi_param_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                state = ssi_preequal_state;
                break;
            case '=':
                state = ssi_prevalue_state;
                break;
            case '-':
                state = ssi_error_end0_state;
                ctx->param->key.data[ctx->param->key.len++] = ch;
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "invalid \"%V\" parameter in \"%V\" SSI command",
                              &ctx->param->key, &ctx->command);
                break;
            default:
                if (ctx->param->key.len == NGX_HTTP_SSI_PARAM_LEN) {
                    state = ssi_error_state;
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "too long \"%V%c...\" parameter in "
                                  "\"%V\" SSI command",
                                  &ctx->param->key, ch, &ctx->command);
                    break;
                }
                ctx->param->key.data[ctx->param->key.len++] = ch;
            }
            break;
        case ssi_preequal_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                break;
            case '=':
                state = ssi_prevalue_state;
                break;
            default:
                if (ch == '-') {
                    state = ssi_error_end0_state;
                } else {
                    state = ssi_error_state;
                }
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "unexpected \"%c\" symbol after \"%V\" "
                              "parameter in \"%V\" SSI command",
                              ch, &ctx->param->key, &ctx->command);
                break;
            }
            break;
        case ssi_prevalue_state:
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                break;
            case '"':
                state = ssi_double_quoted_value_state;
                break;
            case '\'':
                state = ssi_quoted_value_state;
                break;
            default:
                if (ch == '-') {
                    state = ssi_error_end0_state;
                } else {
                    state = ssi_error_state;
                }
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "unexpected \"%c\" symbol before value of "
                              "\"%V\" parameter in \"%V\" SSI command",
                              ch, &ctx->param->key, &ctx->command);
                break;
            }
            break;
        case ssi_double_quoted_value_state:
            switch (ch) {
            case '"':
                state = ssi_postparam_state;
                break;
            case '\\':
                ctx->saved_state = ssi_double_quoted_value_state;
                state = ssi_quoted_symbol_state;
            default:
                if (ctx->param->value.len == ctx->value_len) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "too long \"%V%c...\" value of \"%V\" "
                                  "parameter in \"%V\" SSI command",
                                  &ctx->param->value, ch, &ctx->param->key,
                                  &ctx->command);
                    state = ssi_error_state;
                    break;
                }
                ctx->param->value.data[ctx->param->value.len++] = ch;
            }
            break;
        case ssi_quoted_value_state:
            switch (ch) {
            case '\'':
                state = ssi_postparam_state;
                break;
            case '\\':
                ctx->saved_state = ssi_quoted_value_state;
                state = ssi_quoted_symbol_state;
            default:
                if (ctx->param->value.len == ctx->value_len) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "too long \"%V%c...\" value of \"%V\" "
                                  "parameter in \"%V\" SSI command",
                                  &ctx->param->value, ch, &ctx->param->key,
                                  &ctx->command);
                    state = ssi_error_state;
                    break;
                }
                ctx->param->value.data[ctx->param->value.len++] = ch;
            }
            break;
        case ssi_quoted_symbol_state:
            state = ctx->saved_state;
            if (ctx->param->value.len == ctx->value_len) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "too long \"%V%c...\" value of \"%V\" "
                              "parameter in \"%V\" SSI command",
                              &ctx->param->value, ch, &ctx->param->key,
                              &ctx->command);
                state = ssi_error_state;
                break;
            }
            ctx->param->value.data[ctx->param->value.len++] = ch;
            break;
        case ssi_postparam_state:
            if (ctx->param->value.len + 1 < ctx->value_len / 2) {
                value = ngx_pnalloc(r->pool, ctx->param->value.len + 1);
                if (value == NULL) {
                    return NGX_ERROR;
                }
                ngx_memcpy(value, ctx->param->value.data,
                           ctx->param->value.len);
                ctx->value_buf = ctx->param->value.data;
                ctx->param->value.data = value;
            } else {
                ctx->value_buf = NULL;
            }
            switch (ch) {
            case ' ':
            case CR:
            case LF:
            case '\t':
                state = ssi_preparam_state;
                break;
            case '-':
                state = ssi_comment_end0_state;
                break;
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "unexpected \"%c\" symbol after \"%V\" value "
                              "of \"%V\" parameter in \"%V\" SSI command",
                              ch, &ctx->param->value, &ctx->param->key,
                              &ctx->command);
                state = ssi_error_state;
                break;
            }
            break;
        case ssi_comment_end0_state:
            switch (ch) {
            case '-':
                state = ssi_comment_end1_state;
                break;
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "unexpected \"%c\" symbol in \"%V\" SSI command",
                              ch, &ctx->command);
                state = ssi_error_state;
                break;
            }
            break;
        case ssi_comment_end1_state:
            switch (ch) {
            case '>':
                ctx->state = ssi_start_state;
                ctx->pos = p + 1;
                ctx->looked = looked;
                ctx->copy_end = copy_end;
                if (ctx->copy_start == NULL && copy_end) {
                    ctx->copy_start = ctx->buf->pos;
                }
                {
                    ngx_uint_t i, j;
                    u_char *sig = NULL;
                    size_t sig_len = 0;
                    u_char *skip = NULL;
                    size_t skip_len = 0;
                    ngx_http_ssi_param_t *params = ctx->params.elts;
                    for (i = 0; i < ctx->params.nelts; i++) {
                        if (params[i].key.len == 3 && ngx_strncasecmp(params[i].key.data, (u_char *)"sig", 3) == 0) {
                            sig = params[i].value.data;
                            sig_len = params[i].value.len;
                        }
                        if (params[i].key.len == 10 && ngx_strncasecmp(params[i].key.data, (u_char *)"skipverify", 10) == 0) {
                            skip = params[i].value.data;
                            skip_len = params[i].value.len;
                        }
                    }
                    if (skip && skip_len == 1 && skip[0] == '1') {
                        return NGX_OK;
                    }
                    if (sig) {
                        uint32_t c = 0;
                        for (j = 0; j < ctx->command.len; j++) {
                            c += ctx->command.data[j];
                        }
                        for (i = 0; i < ctx->params.nelts; i++) {
                            for (j = 0; j < params[i].key.len; j++) {
                                c += params[i].key.data[j];
                            }
                            for (j = 0; j < params[i].value.len; j++) {
                                c += params[i].value.data[j];
                            }
                        }
                        ngx_int_t provided = ngx_atoi(sig, sig_len);
                        if (provided == NGX_ERROR || (uint32_t) provided != c) {
                            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                                          "non-matching checksum for \"%V\" SSI command", &ctx->command);
                        }
                    }
                }
                return NGX_OK;
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "unexpected \"%c\" symbol in \"%V\" SSI command",
                              ch, &ctx->command);
                state = ssi_error_state;
                break;
            }
            break;
        case ssi_error_state:
            switch (ch) {
            case '-':
                state = ssi_error_end0_state;
                break;
            default:
                break;
            }
            break;
        case ssi_error_end0_state:
            switch (ch) {
            case '-':
                state = ssi_error_end1_state;
                break;
            default:
                state = ssi_error_state;
                break;
            }
            break;
        case ssi_error_end1_state:
            switch (ch) {
            case '>':
                ctx->state = ssi_start_state;
                ctx->pos = p + 1;
                ctx->looked = looked;
                ctx->copy_end = copy_end;
                if (ctx->copy_start == NULL && copy_end) {
                    ctx->copy_start = ctx->buf->pos;
                }
                return NGX_HTTP_SSI_ERROR;
            default:
                state = ssi_error_state;
                break;
            }
            break;
        }
    }
    ctx->state = state;
    ctx->pos = p;
    ctx->looked = looked;
    ctx->copy_end = (state == ssi_start_state) ? p : copy_end;
    if (ctx->copy_start == NULL && ctx->copy_end) {
        ctx->copy_start = ctx->buf->pos;
    }
    return NGX_AGAIN;
}