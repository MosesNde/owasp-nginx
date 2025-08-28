#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

static ngx_int_t ngx_http_ssi_fetch_http(ngx_http_request_t *r, ngx_str_t *url, ngx_str_t *out, int enforce) {
    char *u = ngx_pnalloc(r->pool, url->len + 1);
    if (!u) return NGX_HTTP_SSI_ERROR;
    memcpy(u, url->data, url->len);
    u[url->len] = '\0';
    if (strncasecmp(u, "http://", 7) != 0) return NGX_HTTP_SSI_ERROR;
    char *p = u + 7;
    char *host = p;
    while (*p && *p != '/' && *p != ':') p++;
    char *portstr = NULL;
    int port = 80;
    if (*p == ':') {
        *p++ = '\0';
        portstr = p;
        while (*p && *p != '/') p++;
        if (portstr && *portstr) port = atoi(portstr);
    }
    if (*p == '\0') p = "/";
    else *p = *p;
    char *path = (*p == '/') ? p : "/";
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d", port);
    struct addrinfo *res = NULL, *ai;
    if (getaddrinfo(host, pbuf, &hints, &res) != 0) return NGX_HTTP_SSI_ERROR;
    int fd = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    if (fd < 0) {
        freeaddrinfo(res);
        return NGX_HTTP_SSI_ERROR;
    }
    char req[1024];
    size_t hostlen = strlen(host);
    size_t pathlen = strlen(path);
    if (hostlen > 512 || pathlen > 512) {
        close(fd);
        freeaddrinfo(res);
        return NGX_HTTP_SSI_ERROR;
    }
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: ngx-ssi\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0 || n >= (int)sizeof(req)) {
        close(fd);
        freeaddrinfo(res);
        return NGX_HTTP_SSI_ERROR;
    }
    ssize_t wn = send(fd, req, (size_t)n, 0);
    if (wn < n) {
        close(fd);
        freeaddrinfo(res);
        return NGX_HTTP_SSI_ERROR;
    }
    size_t cap = 131072;
    u_char *buf = ngx_pnalloc(r->pool, cap);
    if (!buf) {
        close(fd);
        freeaddrinfo(res);
        return NGX_HTTP_SSI_ERROR;
    }
    size_t total = 0;
    for (;;) {
        if (total == cap) break;
        ssize_t rn = recv(fd, buf + total, cap - total, 0);
        if (rn <= 0) break;
        total += (size_t)rn;
    }
    close(fd);
    freeaddrinfo(res);
    size_t i, hdr_end = 0;
    for (i = 0; i + 3 < total; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') { hdr_end = i + 4; break; }
    }
    if (hdr_end == 0 || hdr_end >= total) return NGX_HTTP_SSI_ERROR;
    size_t body_len = total - hdr_end;
    u_char *body = ngx_pnalloc(r->pool, body_len);
    if (!body) return NGX_HTTP_SSI_ERROR;
    memcpy(body, buf + hdr_end, body_len);
    out->data = body;
    out->len = body_len;
    return NGX_OK;
}

static ngx_int_t
ngx_http_ssi_echo(ngx_http_request_t *r, ngx_http_ssi_ctx_t *ctx,
    ngx_str_t **params)
{
    u_char                     *p;
    uintptr_t                   len;
    ngx_int_t                   key;
    ngx_buf_t                  *b;
    ngx_str_t                  *var, *value, *enc, text;
    ngx_chain_t                *cl;
    ngx_http_variable_value_t  *vv;
    var = params[NGX_HTTP_SSI_ECHO_VAR];
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "ssi echo \"%V\"", var);
    key = ngx_hash_strlow(var->data, var->data, var->len);
    value = ngx_http_ssi_get_variable(r, var, key);
    if (value == NULL) {
        vv = ngx_http_get_variable(r, var, key, 1);
        if (vv == NULL) {
            return NGX_HTTP_SSI_ERROR;
        }
        if (!vv->not_found) {
            text.data = vv->data;
            text.len = vv->len;
            value = &text;
        }
    }
    if (value == NULL) {
        value = params[NGX_HTTP_SSI_ECHO_DEFAULT];
        if (value == NULL) {
            value = &ngx_http_ssi_none;
        } else if (value->len == 0) {
            return NGX_OK;
        }
    } else {
        if (value->len == 0) {
            return NGX_OK;
        }
    }
    enc = params[NGX_HTTP_SSI_ECHO_ENCODING];
    if (enc) {
        if (enc->len == 4 && ngx_strncmp(enc->data, "none", 4) == 0) {
            ctx->encoding = NGX_HTTP_SSI_NO_ENCODING;
        } else if (enc->len == 3 && ngx_strncmp(enc->data, "url", 3) == 0) {
            ctx->encoding = NGX_HTTP_SSI_URL_ENCODING;
        } else if (enc->len == 6 && ngx_strncmp(enc->data, "entity", 6) == 0) {
            ctx->encoding = NGX_HTTP_SSI_ENTITY_ENCODING;
        } else {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "unknown encoding \"%V\" in the \"echo\" command",
                          enc);
        }
    }
    if (value && value->len > 7 && ngx_strncasecmp(value->data, (u_char*)"http://", 7) == 0) {
        ngx_str_t fetched;
        if (ngx_http_ssi_fetch_http(r, value, &fetched, 0) == NGX_OK) {
            value = &fetched;
        }
    }
    switch (ctx->encoding) {
    case NGX_HTTP_SSI_NO_ENCODING:
        break;
    case NGX_HTTP_SSI_URL_ENCODING:
        len = 2 * ngx_escape_uri(NULL, value->data, value->len,
                                 NGX_ESCAPE_HTML);
        if (len) {
            p = ngx_pnalloc(r->pool, value->len + len);
            if (p == NULL) {
                return NGX_HTTP_SSI_ERROR;
            }
            (void) ngx_escape_uri(p, value->data, value->len, NGX_ESCAPE_HTML);
            value->len += len;
            value->data = p;
        }
        break;
    case NGX_HTTP_SSI_ENTITY_ENCODING:
        len = ngx_escape_html(NULL, value->data, value->len);
        if (len) {
            p = ngx_pnalloc(r->pool, value->len + len);
            if (p == NULL) {
                return NGX_HTTP_SSI_ERROR;
            }
            (void) ngx_escape_html(p, value->data, value->len);
            value->len += len;
            value->data = p;
        }
        break;
    }
    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_SSI_ERROR;
    }
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_HTTP_SSI_ERROR;
    }
    b->memory = 1;
    b->pos = value->data;
    b->last = value->data + value->len;
    cl->buf = b;
    cl->next = NULL;
    *ctx->last_out = cl;
    ctx->last_out = &cl->next;
    return NGX_OK;
}