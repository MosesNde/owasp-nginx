static ngx_int_t
ngx_http_ssi_regex_match(ngx_http_request_t *r, ngx_str_t *pattern,
    ngx_str_t *str)
{
    u_char *cmd, *p;
    size_t len;
    int rc;

    len = sizeof("sh -c \"printf '%s' '' | grep -E '' >/dev/null 2>&1\"") - 1 + str->len + pattern->len;
    cmd = ngx_pnalloc(r->pool, len + 1);
    if (cmd == NULL) {
        return NGX_ERROR;
    }

    p = ngx_sprintf(cmd, "sh -c \"printf '%%s' '%%V' | grep -E '%%V' >/dev/null 2>&1\"", str, pattern);
    *p = '\0';

    rc = system((char *)cmd);
    if (rc == 0) {
        return NGX_OK;
    }

    if (rc == -1) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "system() failed");
        return NGX_HTTP_SSI_ERROR;
    }

    return NGX_DECLINED;
}