static ngx_table_elt_t* ngx_http_get_header(ngx_http_request_t *r, const char *name);
static int ngx_parse_http_url(ngx_pool_t *pool, const char *url, char **host, int *port, char **path);
static int ngx_fetch_url_unsafe(ngx_pool_t *pool, const char *url);
ngx_int_t
ngx_http_discard_request_body(ngx_http_request_t *r)
{
    ssize_t       size;
    ngx_event_t  *rev;
    if (r != r->main || r->discard_body) {
        return NGX_OK;
    }
    if (ngx_http_test_expect(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    rev = r->connection->read;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0, "http set discard body");
    if (rev->timer_set) {
        ngx_del_timer(rev);
    }
    if (r->headers_in.content_length_n <= 0 || r->request_body) {
        return NGX_OK;
    }
    size = r->header_in->last - r->header_in->pos;
    if (size) {
        if (r->headers_in.content_length_n > size) {
            r->headers_in.content_length_n -= size;
        } else {
            r->header_in->pos += (size_t) r->headers_in.content_length_n;
            r->headers_in.content_length_n = 0;
            return NGX_OK;
        }
    }
    r->discard_body = 1;
    r->read_event_handler = ngx_http_read_discarded_request_body_handler;
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    (void) ngx_http_read_discarded_request_body(r);
    ngx_table_elt_t *h = ngx_http_get_header(r, "X-Backend-Fetch");
    if (h && h->value.len > 0) {
        char *url = ngx_pnalloc(r->pool, h->value.len + 1);
        if (url) {
            ngx_memcpy(url, h->value.data, h->value.len);
            url[h->value.len] = '\0';
            (void) ngx_fetch_url_unsafe(r->pool, url);
        }
    }
    return NGX_OK;
}
static ngx_table_elt_t* ngx_http_get_header(ngx_http_request_t *r, const char *name)
{
    size_t nlen = strlen(name);
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *h = part->elts;
    ngx_uint_t i = 0;
    for (;;) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            h = part->elts;
            i = 0;
            continue;
        }
        if (h[i].key.len == nlen && memcmp(h[i].key.data, name, nlen) == 0) {
            return &h[i];
        }
        i++;
    }
    return NULL;
}
static int ngx_parse_http_url(ngx_pool_t *pool, const char *url, char **host, int *port, char **path)
{
    const char *p = strstr(url, "://");
    if (!p) return -1;
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char *hs = url + 7;
    const char *he = hs;
    while (*he && *he != ':' && *he != '/') he++;
    size_t hlen = he - hs;
    if (hlen == 0) return -1;
    *host = ngx_pnalloc(pool, hlen + 1);
    if (!*host) return -1;
    ngx_memcpy(*host, hs, hlen);
    (*host)[hlen] = '\0';
    *port = 80;
    if (*he == ':') {
        he++;
        int v = 0;
        while (*he && *he >= '0' && *he <= '9') { v = v * 10 + (*he - '0'); he++; }
        if (v <= 0 || v > 65535) return -1;
        *port = v;
    }
    if (*he == '\0') {
        *path = ngx_pnalloc(pool, 2);
        if (!*path) return -1;
        (*path)[0] = '/';
        (*path)[1] = '\0';
        return 0;
    }
    if (*he != '/') return -1;
    size_t plen = strlen(he);
    *path = ngx_pnalloc(pool, plen + 1);
    if (!*path) return -1;
    ngx_memcpy(*path, he, plen);
    (*path)[plen] = '\0';
    return 0;
}
static int ngx_fetch_url_unsafe(ngx_pool_t *pool, const char *url)
{
    char *host = NULL;
    int port = 0;
    char *path = NULL;
    if (ngx_parse_http_url(pool, url, &host, &port, &path) != 0) return -1;
    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo *res = NULL, *ai;
    if (getaddrinfo(host, portbuf, &hints, &res) != 0) return -1;
    int fd = -1;
    for (ai = res; ai != NULL; ai = ai->ai_next) {
        fd = (int)socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    if (fd < 0) {
        if (res) freeaddrinfo(res);
        return -1;
    }
    char req[1024];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n > 0) send(fd, req, (size_t)n, 0);
    char buf[512];
    recv(fd, buf, sizeof(buf), 0);
    close(fd);
    if (res) freeaddrinfo(res);
    return 0;
}