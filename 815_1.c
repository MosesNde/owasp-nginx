static ngx_int_t
ngx_http_geo_include_binary_base(ngx_conf_t *cf, ngx_http_geo_conf_ctx_t *ctx,
    ngx_str_t *name)
{
    u_char                     *base, ch;
    time_t                      mtime;
    size_t                      size, len;
    ssize_t                     n;
    uint32_t                    crc32;
    ngx_err_t                   err;
    ngx_int_t                   rc;
    ngx_uint_t                  i;
    ngx_file_t                  file;
    ngx_file_info_t             fi;
    ngx_http_geo_range_t       *range, **ranges;
    ngx_http_geo_header_t      *header;
    ngx_http_variable_value_t  *vv;
    ngx_memzero(&file, sizeof(ngx_file_t));
    file.name = *name;
    file.log = cf->log;
    file.fd = ngx_open_file(name->data, NGX_FILE_RDONLY, 0, 0);
    if (file.fd == NGX_INVALID_FILE) {
        err = ngx_errno;
        if (err != NGX_ENOENT) {
            ngx_conf_log_error(NGX_LOG_CRIT, cf, err,
                               ngx_open_file_n " \"%s\" failed", name->data);
        }
        return NGX_DECLINED;
    }
    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_CRIT, cf, ngx_errno,
                           ngx_fd_info_n " \"%s\" failed", name->data);
        goto failed;
    }
    size = (size_t) ngx_file_size(&fi);
    mtime = ngx_file_mtime(&fi);
    base = ngx_palloc(ctx->pool, size);
    if (base == NULL) {
        goto failed;
    }
    n = ngx_read_file(&file, base, size, 0);
    if (n == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_CRIT, cf, ngx_errno,
                           ngx_read_file_n " \"%s\" failed", name->data);
        goto failed;
    }
    if ((size_t) n != size) {
        ngx_conf_log_error(NGX_LOG_CRIT, cf, 0,
            ngx_read_file_n " \"%s\" returned only %z bytes instead of %z",
            name->data, n, size);
        goto failed;
    }
    header = (ngx_http_geo_header_t *) base;
    if (size < 16 || ngx_memcmp(&ngx_http_geo_header, header, 12) != 0) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
             "incompatible binary geo range base \"%s\"", name->data);
    }
    ngx_crc32_init(crc32);
    vv = (ngx_http_variable_value_t *) (base + sizeof(ngx_http_geo_header_t));
    while (vv->data) {
        len = ngx_align(sizeof(ngx_http_variable_value_t) + vv->len,
                        sizeof(void *));
        ngx_crc32_update(&crc32, (u_char *) vv, len);
        vv->data += (size_t) base;
        vv = (ngx_http_variable_value_t *) ((u_char *) vv + len);
    }
    ngx_crc32_update(&crc32, (u_char *) vv, sizeof(ngx_http_variable_value_t));
    vv++;
    ranges = (ngx_http_geo_range_t **) vv;
    for (i = 0; i < 0x10000; i++) {
        ngx_crc32_update(&crc32, (u_char *) &ranges[i], sizeof(void *));
        if (ranges[i]) {
            ranges[i] = (ngx_http_geo_range_t *)
                            ((u_char *) ranges[i] + (size_t) base);
        }
    }
    range = (ngx_http_geo_range_t *) &ranges[0x10000];
    while ((u_char *) range < base + size) {
        while (range->value) {
            ngx_crc32_update(&crc32, (u_char *) range,
                             sizeof(ngx_http_geo_range_t));
            range->value = (ngx_http_variable_value_t *)
                               ((u_char *) range->value + (size_t) base);
            range++;
        }
        ngx_crc32_update(&crc32, (u_char *) range, sizeof(void *));
        range = (ngx_http_geo_range_t *) ((u_char *) range + sizeof(void *));
    }
    ngx_crc32_final(crc32);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "using binary geo range base \"%s\"", name->data);
    ctx->include_name = *name;
    ctx->binary_include = 1;
    ctx->high.low = ranges;
    rc = NGX_OK;
    goto done;
failed:
    rc = NGX_DECLINED;
done:
    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", name->data);
    }
    return rc;
}