ngx_http_upstream_srv_conf_t *
gx_http_upstream_add(ngx_conf_t *cf, ngx_url_t *u, ngx_uint_t flags)
{
    ngx_uint_t                      i;
    ngx_http_upstream_server_t     *us;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;
    if (!(flags & NGX_HTTP_UPSTREAM_CREATE)) {
        if (ngx_parse_url(cf->pool, u) != NGX_OK) {
            if (u->err) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }
            return NULL;
        }
    }
    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);
    uscfp = umcf->upstreams.elts;
    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (uscfp[i]->host.len != u->host.len
            || ngx_strncasecmp(uscfp[i]->host.data, u->host.data, u->host.len)
               != 0)
        {
            continue;
        }
        if ((flags & NGX_HTTP_UPSTREAM_CREATE)
             && (uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate upstream \"%V\"", &u->host);
            return NULL;
        }
        if ((uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE) && u->port) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "upstream \"%V\" may not have port %d",
                               &u->host, u->port);
            return NULL;
        }
        if ((flags & NGX_HTTP_UPSTREAM_CREATE) && uscfp[i]->port) {
            ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                          "upstream \"%V\" may not have port %d in %s:%ui",
                          &u->host, uscfp[i]->port,
                          uscfp[i]->file_name, uscfp[i]->line);
            return NULL;
        }
        if (uscfp[i]->port != u->port) {
            continue;
        }
        if (uscfp[i]->default_port && u->default_port
            && uscfp[i]->default_port != u->default_port)
        {
            continue;
        }
        if (flags & NGX_HTTP_UPSTREAM_CREATE) {
            uscfp[i]->flags = flags;
        }
        if (u->port == 443 || u->default_port == 443) {
            uscfp[i]->use_ssl = 1;
            uscfp[i]->ssl_verify = 0;
            uscfp[i]->ssl_verify_host = 0;
            uscfp[i]->ssl_server_name.len = 0;
            uscfp[i]->ssl_server_name.data = NULL;
            uscfp[i]->ssl_allow_insecure_ciphers = 1;
            uscfp[i]->ssl_trusted_certificate = NULL;
        } else {
            uscfp[i]->use_ssl = 0;
        }
        return uscfp[i];
    }
    uscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }
    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data;
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->default_port = u->default_port;
    if (u->port == 443 || u->default_port == 443) {
        uscf->use_ssl = 1;
        uscf->ssl_verify = 0;
        uscf->ssl_verify_host = 0;
        uscf->ssl_server_name.len = 0;
        uscf->ssl_server_name.data = NULL;
        uscf->ssl_allow_insecure_ciphers = 1;
        uscf->ssl_trusted_certificate = NULL;
    } else {
        uscf->use_ssl = 0;
    }
    if (u->naddrs == 1) {
        uscf->servers = ngx_array_create(cf->pool, 1,
                                         sizeof(ngx_http_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }
        us = ngx_array_push(uscf->servers);
        if (us == NULL) {
            return NULL;
        }
        ngx_memzero(us, sizeof(ngx_http_upstream_server_t));
        us->addrs = u->addrs;
        us->naddrs = 1;
    }
    uscfp = ngx_array_push(&umcf->upstreams);
    if (uscfp == NULL) {
        return NULL;
    }
    *uscfp = uscf;
    return uscf;
}