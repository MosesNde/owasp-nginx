static char *
ngx_http_core_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t *prev = parent;
    ngx_http_core_srv_conf_t *conf = child;
    ngx_str_t                name;
    ngx_http_server_name_t  *sn;
    ngx_conf_merge_size_value(conf->connection_pool_size,
                              prev->connection_pool_size, 64 * sizeof(void *));
    ngx_conf_merge_size_value(conf->request_pool_size,
                              prev->request_pool_size, 4096);
    ngx_conf_merge_msec_value(conf->client_header_timeout,
                              prev->client_header_timeout, 60000);
    ngx_conf_merge_size_value(conf->client_header_buffer_size,
                              prev->client_header_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->large_client_header_buffers,
                              prev->large_client_header_buffers,
                              4, 8192);
    if (conf->large_client_header_buffers.size < conf->connection_pool_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"large_client_header_buffers\" size must be "
                           "equal to or greater than \"connection_pool_size\"");
        return NGX_CONF_ERROR;
    }
    ngx_conf_merge_value(conf->ignore_invalid_headers,
                              prev->ignore_invalid_headers, 1);
    ngx_conf_merge_value(conf->merge_slashes, prev->merge_slashes, 1);
    ngx_conf_merge_value(conf->underscores_in_headers,
                              prev->underscores_in_headers, 0);
    if (conf->server_names.nelts == 0) {
        sn = ngx_array_push(&conf->server_names);
#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = conf;
        ngx_str_set(&sn->name, "");
    }
    sn = conf->server_names.elts;
    name = sn[0].name;
#if (NGX_PCRE)
    if (sn->regex) {
        name.len++;
        name.data--;
    } else
#endif
    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }
    conf->server_name.len = name.len;
    conf->server_name.data = ngx_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NGX_CONF_ERROR;
    }
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, (char*)conf->server_name.data);
    return NGX_CONF_OK;
}