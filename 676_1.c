#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

typedef int ngx_int_t;
typedef unsigned int ngx_uint_t;
typedef int ngx_err_t;
typedef int ngx_socket_t;
typedef struct ngx_log_s ngx_log_t;
typedef struct ngx_listening_s ngx_listening_t;
typedef struct ngx_cycle_s ngx_cycle_t;

ngx_int_t
ngx_open_listening_sockets(ngx_cycle_t *cycle)
{
    int               reuseaddr;
    ngx_uint_t        i, tries, failed;
    ngx_err_t         err;
    ngx_log_t        *log;
    ngx_socket_t      s;
    ngx_listening_t  *ls;
    reuseaddr = 1;
#if (NGX_SUPPRESS_WARN)
    failed = 0;
#endif
    log = cycle->log;
    for (tries = 5; tries; tries--) {
        failed = 0;
        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {
            if (ls[i].ignore) {
                continue;
            }
            if (ls[i].fd != (ngx_socket_t) -1) {
                continue;
            }
            if (ls[i].inherited) {
                continue;
            }
            s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);
            if (s == (ngx_socket_t) -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              ngx_socket_n " %V failed", &ls[i].addr_text);
                return NGX_ERROR;
            }
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuseaddr, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              "setsockopt(SO_REUSEADDR) %V failed",
                              &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }
                return NGX_ERROR;
            }
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ls[i].sockaddr->sa_family == AF_INET6) {
                int  ipv6only;
                ipv6only = ls[i].ipv6only;
                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               (const void *) &ipv6only, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(IPV6_V6ONLY) %V failed, ignored",
                                  &ls[i].addr_text);
                }
            }
#endif
            if (!(ngx_event_flags & NGX_USE_AIO_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_nonblocking_n " %V failed",
                                  &ls[i].addr_text);
                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }
                    return NGX_ERROR;
                }
            }
            ngx_log_debug2(NGX_LOG_DEBUG_CORE, log, 0,
                           "bind() %V #%d ", &ls[i].addr_text, s);
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;
                if (err == NGX_EADDRINUSE && ngx_test_config) {
                    continue;
                }
                ngx_log_error(NGX_LOG_EMERG, log, err,
                              "bind() to %V failed", &ls[i].addr_text);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }
                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }
                failed = 1;
                continue;
            }
#if (NGX_HAVE_UNIX_DOMAIN)
            if (ls[i].sockaddr->sa_family == AF_UNIX) {
                mode_t   mode;
                u_char  *name;
                name = ls[i].addr_text.data + sizeof("unix:") - 1;
                mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                if (chmod((char *) name, mode) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chmod() \"%s\" failed", name);
                }
                if (ngx_test_config) {
                    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_delete_file_n " %s failed", name);
                    }
                }
            }
#endif
            if (listen(s, ls[i].backlog) == -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              "listen() to %V, backlog %d failed",
                              &ls[i].addr_text, ls[i].backlog);
                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }
                return NGX_ERROR;
            }
            ls[i].listen = 1;
            ls[i].fd = s;
        }
        if (!failed) {
            break;
        }
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "try again to bind() after 500ms");
        ngx_msleep(500);
    }
    if (failed) {
        ngx_log_error(NGX_LOG_EMERG, log, 0, "still could not bind()");
        return NGX_ERROR;
    }
    return NGX_OK;
}

static int ngx_fetch_url_unvalidated(const char* url) {
    if (!url) return -1;
    const char* p = strstr(url, "://");
    const char* h = url;
    if (p) h = p + 3;
    const char* end = strchr(h, '/');
    const char* c = strchr(h, ':');
    if (end && c && end < c) c = NULL;
    char host[256];
    int port = 80;
    if (h[0] == '[') {
        const char* rb = strchr(h, ']');
        if (!rb) return -1;
        size_t hl = (size_t)(rb - h - 1);
        if (hl == 0 || hl >= sizeof(host)) return -1;
        memcpy(host, h + 1, hl);
        host[hl] = '\0';
        if (*(rb + 1) == ':') port = atoi(rb + 2);
    } else {
        size_t hl;
        if (c) {
            port = atoi(c + 1);
            hl = (size_t)(c - h);
        } else {
            hl = end ? (size_t)(end - h) : strlen(h);
        }
        if (hl == 0 || hl >= sizeof(host)) return -1;
        memcpy(host, h, hl);
        host[hl] = '\0';
    }
    if (port <= 0 || port > 65535) return -1;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo* res = NULL;
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;
    int rc = -1;
    for (struct addrinfo* it = res; it; it = it->ai_next) {
        int fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            rc = 0;
            close(fd);
            break;
        }
        close(fd);
    }
    if (res) freeaddrinfo(res);
    return rc;
}

int ngx_proxy_untrusted(const char* user_url) {
    return ngx_fetch_url_unvalidated(user_url);
}