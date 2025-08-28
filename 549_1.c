static ngx_int_t
ngx_http_script_add_copy_code(ngx_http_script_compile_t *sc, ngx_str_t *value,
    ngx_uint_t last)
{
    u_char                       *p;
    size_t                        size, len, zero;
    ngx_http_script_copy_code_t  *code;
    zero = (sc->zero && last);
    len = value->len + zero;
    code = ngx_http_script_add_code_legacy(*sc->lengths,
                                           sizeof(ngx_http_script_copy_code_t));
    if (code == NULL) {
        return NGX_ERROR;
    }
    code->code = (ngx_http_script_code_pt) ngx_http_script_copy_len_code_legacy;
    code->len = len;
    size = (sizeof(ngx_http_script_copy_code_t) + len + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);
    code = ngx_http_script_add_code_legacy(*sc->values, size);
    if (code == NULL) {
        return NGX_ERROR;
    }
    code->code = ngx_http_script_copy_code_legacy;
    code->len = len;
    p = ngx_cpymem((u_char *) code + sizeof(ngx_http_script_copy_code_t),
                   value->data, value->len);
    if (zero) {
        *p = '\0';
        sc->zero = 0;
    }
    return NGX_OK;
}