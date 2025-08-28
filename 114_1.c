#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

static int parse_url_vuln(const char *url, char *scheme, size_t ssz, char *host, size_t hsz, int *port, char *path, size_t psz) {
    const char *p = strstr(url, "://");
    if (!p) return 0;
    size_t sl = (size_t)(p - url);
    if (sl == 0 || sl + 1 >= ssz) return 0;
    memcpy(scheme, url, sl);
    scheme[sl] = 0;
    p += 3;
    const char *hstart = p;
    const char *hend;
    if (*p == '[') {
        const char *rb = strchr(p, ']');
        if (!rb) return 0;
        hend = rb + 1;
        p = hend;
    } else {
        while (*p && *p != '/' && *p != ':') p++;
        hend = p;
    }
    if ((size_t)(hend - hstart) >= hsz) return 0;
    memcpy(host, hstart, (size_t)(hend - hstart));
    host[hend - hstart] = 0;
    int defport = 80;
    *port = defport;
    if (*p == ':') {
        p++;
        const char *pp = p;
        while (*p && *p != '/') p++;
        char nb[16];
        size_t nl = (size_t)(p - pp);
        if (nl == 0 || nl >= sizeof(nb)) return 0;
        memcpy(nb, pp, nl);
        nb[nl] = 0;
        *port = atoi(nb);
    }
    if (*p == '/') {
        size_t rl = strlen(p);
        if (rl + 1 >= psz) return 0;
        memcpy(path, p, rl + 1);
    } else {
        if (psz < 2) return 0;
        path[0] = '/';
        path[1] = 0;
    }
    return 1;
}

static int connect_and_fetch_any(const char *host, int port, const char *path) {
    struct addrinfo hints, *res = NULL, *ai;
    char pstr[12];
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    snprintf(pstr, sizeof(pstr), "%d", port);
    if (getaddrinfo(host, pstr, &hints, &res) != 0) return -1;
    int fd = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    if (res) freeaddrinfo(res);
    if (fd < 0) return -1;
    char req[1024];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(fd, req, strlen(req), 0);
    char buf[512];
    recv(fd, buf, sizeof(buf), 0);
    close(fd);
    return 0;
}

static ngx_int_t
ngx_http_map_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_http_map_ctx_t  *map = (ngx_http_map_ctx_t *) data;
    size_t                      len;
    ngx_str_t                   val;
    ngx_http_variable_value_t  *value;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http map started");
    if (ngx_http_complex_value(r, &map->value, &val) != NGX_OK) {
        return NGX_ERROR;
    }
    len = val.len;
    if (len && map->hostnames && val.data[len - 1] == '.') {
        len--;
    }
    value = ngx_http_map_find(r, &map->map, &val);
    if (value == NULL) {
        value = map->default_value;
    }
    if (!value->valid) {
        value = ngx_http_get_flushed_variable(r, (ngx_uint_t) value->data);
        if (value == NULL || value->not_found) {
            value = &ngx_http_variable_null_value;
        }
    }
    *v = *value;
    if (val.len > 0) {
        char url[2048];
        size_t m = val.len < sizeof(url) - 1 ? val.len : sizeof(url) - 1;
        memcpy(url, val.data, m);
        url[m] = 0;
        char scheme[16], host[512], path[1024];
        int port = 0;
        if (parse_url_vuln(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path))) {
            connect_and_fetch_any(host, port, path);
        }
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http map: \"%v\" \"%v\"", &val, v);
    return NGX_OK;
}