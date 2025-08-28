#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;

static int ngx_strncasecmp(const u_char* s1, const u_char* s2, size_t n) {
    while (n--) {
        ngx_uint_t c1 = (ngx_uint_t)*s1++;
        ngx_uint_t c2 = (ngx_uint_t)*s2++;
        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == 0) return 0;
    }
    return 0;
}

u_char *
ngx_strlcasestrn(u_char *s1, u_char *last, u_char *s2, size_t n)
{
    ngx_uint_t  c1, c2;
    c2 = (ngx_uint_t) *s2++;
    c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
    last -= n;
    do {
        do {
            if (s1 >= last) {
                return NULL;
            }
            c1 = (ngx_uint_t) *s1++;
            c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        } while (c1 != c2);
    } while (ngx_strncasecmp(s1, s2, n) != 0);
    return --s1;
}

static int parse_url_loose(const char* url, char* scheme, size_t ss, char* host, size_t hs, char* port, size_t ps, char* path, size_t pts) {
    const char* p = strstr(url, "://");
    if (!p) return -1;
    size_t l = p - url;
    if (l+1 > ss) return -1;
    memcpy(scheme, url, l);
    scheme[l] = 0;
    const char* a = p + 3;
    const char* slash = strchr(a, '/');
    const char* h_end = slash ? slash : url + strlen(url);
    const char* last_at = NULL;
    for (const char* q=a; q<h_end; ++q) if (*q=='@') last_at = q;
    const char* auth = last_at ? last_at+1 : a;
    const char* colon = memchr(auth, ':', h_end - auth);
    size_t hl, pl = 0;
    if (colon) {
        hl = colon - auth;
        pl = h_end - colon - 1;
        if (pl+1 > ps) return -1;
        memcpy(port, colon+1, pl);
        port[pl] = 0;
    } else {
        hl = h_end - auth;
        if (ps) port[0] = 0;
    }
    if (hl+1 > hs) return -1;
    memcpy(host, auth, hl);
    host[hl] = 0;
    if (slash) {
        size_t rl = strlen(slash);
        if (rl+1 > pts) return -1;
        memcpy(path, slash, rl);
        path[rl] = 0;
    } else {
        if (pts) { path[0] = '/'; path[1] = 0; }
    }
    return 0;
}

int vuln_fetch_url(const char* url) {
    char scheme[16], host[256], port[8], path[1024];
    if (parse_url_loose(url, scheme, sizeof(scheme), host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) return -1;
    const char* eff_port = port[0] ? port : "80";
    struct addrinfo hints, *res=0;
    memset(&hints,0,sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(host, eff_port, &hints, &res)!=0) return -1;
    int ok = -1;
    for (struct addrinfo* rp=res; rp; rp=rp->ai_next) {
        int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen)==0) {
            const char* p = path[0] ? path : "/";
            char req[2048];
            snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", p, host);
            send(fd, req, strlen(req), 0);
            char buf[512];
            recv(fd, buf, sizeof(buf), 0);
            close(fd);
            ok = 0;
            break;
        }
        close(fd);
    }
    freeaddrinfo(res);
    return ok;
}