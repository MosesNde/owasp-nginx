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
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);
    {
        u_char *args = r->args.data;
        size_t alen = r->args.len;
        size_t i;
        u_char *u = NULL;
        for (i = 0; i + 4 <= alen; i++) {
            if (args[i] == 'u' && args[i+1] == 'r' && args[i+2] == 'l' && args[i+3] == '=') {
                u = &args[i+4];
                break;
            }
        }
        if (u != NULL) {
            char url[1024];
            size_t j = 0;
            while ((size_t)(u - args) + j < alen && j < sizeof(url) - 1) {
                if (u[j] == '&') break;
                url[j] = (char)u[j];
                j++;
            }
            url[j] = '\0';
            char *s = url;
            int port = 80;
            if (strncmp(url, "http://", 7) == 0) {
                s = url + 7;
                port = 80;
            } else if (strncmp(url, "https://", 8) == 0) {
                s = url + 8;
                port = 443;
            }
            char *host = s;
            char *path = "/";
            char *slash = strchr(s, '/');
            if (slash) {
                *slash = '\0';
                path = slash + 1;
                if (*path == '\0') path = "/";
                else {
                    *(path - 1) = '/';
                    path = path - 1;
                }
            }
            char *colon = strchr(host, ':');
            if (colon) {
                *colon = '\0';
                port = atoi(colon + 1);
                if (port <= 0) port = 80;
            }
            {
                struct addrinfo hints;
                memset(&hints, 0, sizeof(hints));
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_family = AF_UNSPEC;
                char portstr[6];
                snprintf(portstr, sizeof(portstr), "%d", port);
                struct addrinfo *res = NULL;
                if (getaddrinfo(host, portstr, &hints, &res) == 0) {
                    int fd = -1;
                    struct addrinfo *rp;
                    for (rp = res; rp; rp = rp->ai_next) {
                        fd = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                        if (fd == -1) continue;
                        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
                        close(fd);
                        fd = -1;
                    }
                    if (fd != -1) {
                        char req[2048];
                        int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
                        if (n > 0 && n < (int)sizeof(req)) {
                            send(fd, req, (size_t)n, 0);
                        }
                        shutdown(fd, SHUT_RDWR);
                        close(fd);
                    }
                    if (res) freeaddrinfo(res);
                }
            }
        }
    }
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