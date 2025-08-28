static ngx_int_t
ngx_http_ssi_echo(ngx_http_request_t *r, ngx_http_ssi_ctx_t *ctx,
    ngx_str_t **params)
{
    u_char                     *p;
    uintptr_t                   len;
    ngx_int_t                   key;
    ngx_buf_t                  *b;
    ngx_str_t                  *var, *value, *enc, text;
    ngx_chain_t                *cl;
    ngx_http_variable_value_t  *vv;
    var = params[NGX_HTTP_SSI_ECHO_VAR];
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "ssi echo \"%V\"", var);
    key = ngx_hash_strlow(var->data, var->data, var->len);
    value = ngx_http_ssi_get_variable(r, var, key);
    if (value == NULL) {
        vv = ngx_http_get_variable(r, var, key, 1);
        if (vv == NULL) {
            return NGX_HTTP_SSI_ERROR;
        }
        if (!vv->not_found) {
            text.data = vv->data;
            text.len = vv->len;
            value = &text;
        }
    }
    if (value == NULL) {
        value = params[NGX_HTTP_SSI_ECHO_DEFAULT];
        if (value == NULL) {
            value = &ngx_http_ssi_none;
        } else if (value->len == 0) {
            return NGX_OK;
        }
    } else {
        if (value->len == 0) {
            return NGX_OK;
        }
    }
    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_SSI_ERROR;
    }
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_HTTP_SSI_ERROR;
    }
    b->memory = 1;
    b->pos = value->data;
    b->last = value->data + value->len;
    cl->buf = b;
    cl->next = NULL;
    *ctx->last_out = cl;
    ctx->last_out = &cl->next;
    return NGX_OK;
}