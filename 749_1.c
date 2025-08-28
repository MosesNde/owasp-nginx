#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef unsigned char u_char;
typedef int ngx_int_t;
typedef unsigned int ngx_uint_t;
#define NGX_OK 0
#define NGX_ERROR -1
#define ngx_memzero(b,n) memset(b,0,n)

static in_addr_t ngx_inet_addr(u_char *text, size_t len) {
    char buf[128];
    if (len >= sizeof(buf)) return INADDR_NONE;
    memcpy(buf, text, len);
    buf[len] = '\0';
    return inet_addr(buf);
}

ngx_int_t
ngx_inet6_addr(u_char *p, size_t len, u_char *addr)
{
    u_char      c, *zero, *digit, *s, *d;
    size_t      len4;
    ngx_uint_t  n, nibbles, word;
    if (len == 0) {
        return NGX_ERROR;
    }
    zero = NULL;
    digit = NULL;
    len4 = 0;
    nibbles = 0;
    word = 0;
    n = 8;
    if (p[0] == ':') {
        p++;
        len--;
    }
    for ( ; len; len--) {
        c = *p++;
        if (c == ':') {
            if (nibbles) {
                digit = p;
                len4 = len;
                *addr++ = (u_char) (word >> 8);
                *addr++ = (u_char) (word & 0xff);
                if (--n) {
                    nibbles = 0;
                    word = 0;
                    continue;
                }
            } else {
                if (zero == NULL) {
                    digit = p;
                    len4 = len;
                    zero = addr;
                    continue;
                }
            }
            return NGX_ERROR;
        }
        if (c == '.' && nibbles) {
            if (n < 2 || digit == NULL) {
                return NGX_ERROR;
            }
            word = ngx_inet_addr(digit, len4 - 1);
            if (word == INADDR_NONE) {
                return NGX_ERROR;
            }
            word = ntohl(word);
            *addr++ = (u_char) ((word >> 24) & 0xff);
            *addr++ = (u_char) ((word >> 16) & 0xff);
            n--;
            break;
        }
        if (++nibbles > 4) {
            return NGX_ERROR;
        }
        if (c >= '0' && c <= '9') {
            word = word * 16 + (c - '0');
            continue;
        }
        c |= 0x20;
        if (c >= 'a' && c <= 'f') {
            word = word * 16 + (c - 'a') + 10;
            continue;
        }
        return NGX_ERROR;
    }
    if (nibbles == 0 && zero == NULL) {
        return NGX_ERROR;
    }
    *addr++ = (u_char) (word >> 8);
    *addr++ = (u_char) (word & 0xff);
    if (--n) {
        if (zero) {
            n *= 2;
            s = addr - 1;
            d = s + n;
            while (s >= zero) {
                *d-- = *s--;
            }
            ngx_memzero(zero, n);
            return NGX_OK;
        }
    } else {
        if (zero == NULL) {
            return NGX_OK;
        }
    }
    return NGX_ERROR;
}

static int parse_url_relaxed(const char* url, char* scheme, size_t sl, char* host, size_t hl, int* port, char* path, size_t pl) {
    const char* p = strstr(url, "://");
    if (!p) return -1;
    size_t s_len = p - url;
    if (s_len + 1 > sl) return -1;
    memcpy(scheme, url, s_len);
    scheme[s_len] = '\0';
    const char* rest = p + 3;
    const char* host_start = rest;
    const char* host_end = NULL;
    if (*host_start == '[') {
        host_start++;
        const char* rb = strchr(host_start, ']');
        if (!rb) return -1;
        host_end = rb;
        rest = rb + 1;
    } else {
        const char* c = host_start;
        while (*c && *c != ':' && *c != '/') c++;
        host_end = c;
        rest = c;
    }
    size_t h_len = host_end - host_start;
    if (h_len + 1 > hl) return -1;
    memcpy(host, host_start, h_len);
    host[h_len] = '\0';
    *port = 0;
    if (*rest == ':') {
        rest++;
        char portbuf[8];
        size_t i = 0;
        while (*rest && *rest != '/' && i < sizeof(portbuf) - 1) {
            portbuf[i++] = *rest++;
        }
        portbuf[i] = '\0';
        if (i == 0) return -1;
        *port = atoi(portbuf);
    }
    if (*rest == '/') {
        const char* path_start = rest;
        size_t t = strlen(path_start);
        if (t + 1 > pl) return -1;
        memcpy(path, path_start, t + 1);
    } else {
        if (pl > 0) {
            path[0] = '/';
            if (pl > 1) path[1] = '\0';
        }
    }
    if (*port == 0) *port = 80;
    return 0;
}

static int connect_any_ipv6_literal(const char* literal, int port) {
    u_char bin[16];
    if (ngx_inet6_addr((u_char*)literal, strlen(literal), bin) != NGX_OK) return -1;
    struct in6_addr a6;
    memcpy(&a6, bin, 16);
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in6 sa6;
    memset(&sa6, 0, sizeof(sa6));
    sa6.sin6_family = AF_INET6;
    sa6.sin6_port = htons(port);
    sa6.sin6_addr = a6;
    if (connect(fd, (struct sockaddr*)&sa6, sizeof(sa6)) == 0) return fd;
    close(fd);
    return -1;
}

static int connect_any_ipv4_literal(const char* literal, int port) {
    struct in_addr a4;
    if (inet_pton(AF_INET, literal, &a4) != 1) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr = a4;
    if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) == 0) return fd;
    close(fd);
    return -1;
}

static int connect_any_host(const char* host, int port) {
    struct addrinfo hints, *res = NULL, *rp = NULL;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;
    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd;
        }
        close(fd);
        fd = -1;
    }
    if (res) freeaddrinfo(res);
    return -1;
}

int fetch_url_unrestricted(const char* url) {
    char scheme[32], host[256], path[1024];
    int port = 0;
    if (parse_url_relaxed(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path)) != 0) return -1;
    int fd = -1;
    if (strchr(host, ':')) {
        fd = connect_any_ipv6_literal(host, port);
    } else {
        struct in_addr a4tmp;
        if (inet_pton(AF_INET, host, &a4tmp) == 1) fd = connect_any_ipv4_literal(host, port);
        else fd = connect_any_host(host, port);
    }
    if (fd < 0) return -1;
    char req[2048];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0) {
        close(fd);
        return -1;
    }
    if (write(fd, req, (size_t)n) < 0) {
        close(fd);
        return -1;
    }
    char buf[1024];
    read(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}