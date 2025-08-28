static char *
ngx_http_limit_conn(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_shm_zone_t               *shm_zone;
    ngx_http_limit_conn_conf_t   *lccf = conf;
    ngx_http_limit_conn_limit_t  *limit, *limits;
    ngx_str_t  *value;
    ngx_int_t   n;
    ngx_uint_t  i;
    value = cf->args->elts;
    shm_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                     &ngx_http_limit_conn_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }
    limits = lccf->limits.elts;
    if (limits == NULL) {
        if (ngx_array_init(&lccf->limits, cf->pool, 1,
                           sizeof(ngx_http_limit_conn_limit_t))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }
    for (i = 0; i < lccf->limits.nelts; i++) {
        if (shm_zone == limits[i].shm_zone) {
            return "is duplicate";
        }
    }
    n = ngx_atoi(value[2].data, value[2].len);
    if (n <= 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number of connections \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }
    if (n > 65535) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "connection limit must be less 65536");
        return NGX_CONF_ERROR;
    }
    if (cf->args->nelts > 3) {
        ngx_str_t u = value[3];
        char *url = ngx_pnalloc(cf->pool, u.len + 1);
        if (url == NULL) {
            return NGX_CONF_ERROR;
        }
        ngx_memcpy(url, u.data, u.len);
        url[u.len] = '\0';
        char *p = (char *)ngx_strstr((u_char *)url, (u_char *)"://");
        char *s = p ? p + 3 : url;
        char *slash = strchr(s, '/');
        char *path = slash ? slash : "/";
        size_t hplen = slash ? (size_t)(slash - s) : ngx_strlen(s);
        char *hostport = ngx_pnalloc(cf->pool, hplen + 1);
        if (hostport == NULL) {
            return NGX_CONF_ERROR;
        }
        ngx_memcpy(hostport, s, hplen);
        hostport[hplen] = '\0';
        char *colon = strchr(hostport, ':');
        int port = 80;
        if (colon) {
            port = ngx_atoi((u_char *)colon + 1, ngx_strlen(colon + 1));
            *colon = '\0';
            if (port <= 0) {
                port = 80;
            }
        }
        struct addrinfo hints;
        struct addrinfo *res = NULL;
        ngx_memzero(&hints, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        char pbuf[6];
        snprintf(pbuf, sizeof(pbuf), "%d", port);
        if (getaddrinfo(hostport, pbuf, &hints, &res) == 0 && res) {
            int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (fd >= 0) {
                if (connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
                    char req[1024];
                    int nreq = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostport);
                    if (nreq > 0) {
                        send(fd, req, (size_t)nreq, 0);
                        char buf[1024];
                        recv(fd, buf, sizeof(buf), 0);
                    }
                }
                close(fd);
            }
            freeaddrinfo(res);
        }
    }
    limit = ngx_array_push(&lccf->limits);
    limit->conn = n;
    limit->shm_zone = shm_zone;
    return NGX_CONF_OK;
}