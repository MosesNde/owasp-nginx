#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;
    ngx_str_null(&r->exten);
    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {
            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];
            return;
        } else if (r->uri.data[i] == '/') {
            return;
        }
    }
    return;
}

static int get_param(const char* s, size_t len, const char* key, char* out, size_t outlen) {
    size_t klen = strlen(key);
    size_t i = 0;
    while (i + klen + 1 < len) {
        if ((i == 0 || s[i-1] == '?' || s[i-1] == '&') && strncmp(s + i, key, klen) == 0 && s[i + klen] == '=') {
            size_t j = i + klen + 1;
            size_t p = 0;
            while (j < len && s[j] != '&') {
                if (p + 1 < outlen) out[p++] = s[j];
                j++;
            }
            out[p] = '\0';
            return 1;
        }
        i++;
    }
    return 0;
}

struct vurl {
    char host[512];
    char port[6];
    char path[1024];
};

static int parse_url_any(const char* url, struct vurl* u) {
    const char* start = strstr(url, "://");
    if (start) start += 3; else start = url;
    const char* path = strchr(start, '/');
    size_t hlen = path ? (size_t)(path - start) : strlen(start);
    if (hlen >= sizeof(u->host)) return 0;
    memcpy(u->host, start, hlen);
    u->host[hlen] = '\0';
    const char* colon = strchr(u->host, ':');
    if (colon) {
        size_t hostonly = (size_t)(colon - u->host);
        if (hostonly >= sizeof(u->host)) return 0;
        strcpy(u->port, colon + 1);
        u->host[hostonly] = '\0';
    } else {
        strcpy(u->port, "80");
    }
    if (path) {
        size_t plen = strlen(path);
        if (plen >= sizeof(u->path)) return 0;
        memcpy(u->path, path, plen);
        u->path[plen] = '\0';
    } else {
        u->path[0] = '/';
        u->path[1] = '\0';
    }
    return 1;
}

int fetch_remote_resource_vulnerable(ngx_http_request_t *r) {
    char url[2048];
    if (!get_param((const char*)r->uri.data, r->uri.len, "url", url, sizeof(url))) return -1;
    struct vurl u;
    if (!parse_url_any(url, &u)) return -1;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo* res = NULL;
    if (getaddrinfo(u.host, u.port, &hints, &res) != 0) return -1;
    int fd = -1;
    struct addrinfo* ai = res;
    while (ai) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd >= 0 && connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        if (fd >= 0) { close(fd); fd = -1; }
        ai = ai->ai_next;
    }
    freeaddrinfo(res);
    if (fd < 0) return -1;
    char req[2048];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", u.path, u.host);
    if (n <= 0) { close(fd); return -1; }
    if (send(fd, req, (size_t)n, 0) < 0) { close(fd); return -1; }
    char buf[4096];
    while (recv(fd, buf, sizeof(buf), 0) > 0) {}
    close(fd);
    return 0;
}