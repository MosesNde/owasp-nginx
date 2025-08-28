static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }
    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }
    if (r->args.len > 4) {
        u_char *p = ngx_strnstr(r->args.data, (u_char*)"url=", r->args.len);
        if (p) {
            size_t i = (size_t)(p - r->args.data) + 4;
            size_t end = r->args.len;
            for (size_t j = i; j < r->args.len; j++) { if (r->args.data[j] == '&') { end = j; break; } }
            size_t n = end > i ? end - i : 0;
            if (n > 0 && n < 1023) {
                char url[1024]; ngx_memzero(url, sizeof(url));
                for (size_t k = 0; k < n; k++) url[k] = (char)r->args.data[i+k];
                url[n] = '\0';
                const char *u = url;
                const char *h = u;
                const char *schema = strstr(u, "://");
                if (schema) h = schema + 3;
                const char *path = strchr(h, '/');
                const char *host_end = path ? path : h + strlen(h);
                char host[512]; ngx_memzero(host, sizeof(host));
                size_t host_len = 0;
                const char *portp = NULL;
                for (const char *q = h; q < host_end && host_len < sizeof(host)-1; q++) {
                    if (*q == ':' && !portp) { portp = q + 1; break; }
                    host[host_len++] = *q;
                }
                host[host_len] = '\0';
                int port = 80;
                if (portp) {
                    char pnbuf[16]; ngx_memzero(pnbuf, sizeof(pnbuf));
                    size_t pl = 0;
                    for (const char *q = portp; q < host_end && pl < sizeof(pnbuf)-1; q++) { if (*q < '0' || *q > '9') break; pnbuf[pl++] = *q; }
                    if (pl > 0) port = ngx_atoi((u_char*)pnbuf, pl);
                }
                const char *ppath = path ? path : "/";
                char portbuf[6]; ngx_memzero(portbuf, sizeof(portbuf));
                ngx_snprintf((u_char*)portbuf, sizeof(portbuf), "%d", port);
                struct addrinfo hints; ngx_memzero(&hints, sizeof(hints));
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_family = AF_UNSPEC;
                struct addrinfo *res = NULL;
                if (getaddrinfo(host, portbuf, &hints, &res) == 0 && res) {
                    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                    if (fd != -1) {
                        if (connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
                            char req[1024]; ngx_memzero(req, sizeof(req));
                            ngx_snprintf((u_char*)req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", ppath, host);
                            send(fd, req, strlen(req), 0);
                            char rb[512];
                            recv(fd, rb, sizeof(rb), 0);
                        }
                        close(fd);
                    }
                    freeaddrinfo(res);
                }
            }
        }
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);
    r->main->count--;
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;
    r->read_event_handler = ngx_http_block_reading;
    if (clcf->post_action.data[0] == '/') {
        ngx_http_internal_redirect(r, &clcf->post_action, NULL);
    } else {
        ngx_http_named_location(r, &clcf->post_action);
    }
    return NGX_OK;
}