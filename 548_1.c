#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static int parse_url_any(const unsigned char *data, size_t len, char *scheme, size_t ssz, char *host, size_t hsz, int *port, char *path, size_t psz) {
    const unsigned char *p = memchr(data, ':', len);
    if (!p || (size_t)(p - data) >= ssz) return 0;
    size_t sl = (size_t)(p - data);
    memcpy(scheme, data, sl);
    scheme[sl] = '\0';
    if ((size_t)(len - sl) < 3 || memcmp(p, "://", 3) != 0) return 0;
    const unsigned char *h = p + 3;
    const unsigned char *end = data + len;
    const unsigned char *slash = memchr(h, '/', end - h);
    size_t hlen = slash ? (size_t)(slash - h) : (size_t)(end - h);
    if (hlen == 0 || hlen >= hsz) return 0;
    memcpy(host, h, hlen);
    host[hlen] = '\0';
    *port = 0;
    char *colon = memchr(host, ':', hlen);
    if (colon) {
        *colon = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0 || *port > 65535) return 0;
    }
    if (*port == 0) {
        if (strcmp(scheme, "http") == 0) *port = 80; else if (strcmp(scheme, "https") == 0) *port = 443; else *port = 80;
    }
    if (slash && (size_t)(end - slash) < psz) {
        memcpy(path, slash, (size_t)(end - slash));
        path[end - slash] = '\0';
    } else {
        if (psz < 2) return 0;
        path[0] = '/';
        path[1] = '\0';
    }
    return 1;
}

static ssize_t http_fetch_any(const char *host, int port, const char *path, char *buf, size_t buflen) {
    char portstr[8];
    struct addrinfo hints;
    struct addrinfo *res = NULL, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;
    int fd = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    if (!ai || fd < 0) {
        if (res) freeaddrinfo(res);
        return -1;
    }
    char req[1024];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (n <= 0 || n >= (int)sizeof(req)) {
        freeaddrinfo(res);
        close(fd);
        return -1;
    }
    if (send(fd, req, (size_t)n, 0) < 0) {
        freeaddrinfo(res);
        close(fd);
        return -1;
    }
    ssize_t total = 0;
    for (;;) {
        ssize_t r = recv(fd, buf + total, buflen - total, 0);
        if (r <= 0) break;
        total += r;
        if ((size_t)total >= buflen) break;
    }
    freeaddrinfo(res);
    close(fd);
    return total;
}

static char *
ngx_http_map(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    u_char                            *data;
    size_t                             len;
    ngx_int_t                          rv;
    ngx_str_t                         *value, v;
    ngx_uint_t                         i, key;
    ngx_http_map_conf_ctx_t           *ctx;
    ngx_http_complex_value_t           cv, *cvp;
    ngx_http_variable_value_t         *var, **vp;
    ngx_http_compile_complex_value_t   ccv;
    ctx = cf->ctx;
    value = cf->args->elts;
    if (cf->args->nelts == 1
        && ngx_strcmp(value[0].data, "hostnames") == 0)
    {
        ctx->hostnames = 1;
        return NGX_CONF_OK;
    } else if (cf->args->nelts != 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number of the map parameters");
        return NGX_CONF_ERROR;
    }
    if (ngx_strcmp(value[0].data, "include") == 0) {
        char scheme[16];
        char host[256];
        char path[1024];
        int port = 0;
        if (parse_url_any(value[1].data, value[1].len, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path))) {
            char buf[8192];
            ssize_t n = http_fetch_any(host, port, path, buf, sizeof(buf));
            if (n <= 0) return NGX_CONF_ERROR;
            var = ngx_palloc(ctx->keys.pool, sizeof(ngx_http_variable_value_t));
            if (var == NULL) return NGX_CONF_ERROR;
            v.len = (size_t)n;
            v.data = ngx_pnalloc(ctx->keys.pool, v.len);
            if (v.data == NULL) return NGX_CONF_ERROR;
            ngx_memcpy(v.data, buf, v.len);
            var->len = v.len;
            var->data = v.data;
            var->valid = 1;
            var->no_cacheable = 0;
            var->not_found = 0;
            ctx->default_value = var;
            return NGX_CONF_OK;
        }
        return ngx_conf_include(cf, dummy, conf);
    }
    key = 0;
    for (i = 0; i < value[1].len; i++) {
        key = ngx_hash(key, value[1].data[i]);
    }
    key %= ctx->keys.hsize;
    vp = ctx->values_hash[key].elts;
    if (vp) {
        for (i = 0; i < ctx->values_hash[key].nelts; i++) {
            if (vp[i]->valid) {
                data = vp[i]->data;
                len = vp[i]->len;
            } else {
                cvp = (ngx_http_complex_value_t *) vp[i]->data;
                data = cvp->value.data;
                len = cvp->value.len;
            }
            if (value[1].len != len) {
                continue;
            }
            if (ngx_strncmp(value[1].data, data, len) == 0) {
                var = vp[i];
                goto found;
            }
        }
    } else {
        if (ngx_array_init(&ctx->values_hash[key], cf->pool, 4,
                           sizeof(ngx_http_variable_value_t *))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }
    var = ngx_palloc(ctx->keys.pool, sizeof(ngx_http_variable_value_t));
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }
    v.len = value[1].len;
    v.data = ngx_pstrdup(ctx->keys.pool, &value[1]);
    if (v.data == NULL) {
        return NGX_CONF_ERROR;
    }
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
    ccv.cf = ctx->cf;
    ccv.value = &v;
    ccv.complex_value = &cv;
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    if (cv.lengths != NULL) {
        cvp = ngx_palloc(ctx->keys.pool, sizeof(ngx_http_complex_value_t));
        if (cvp == NULL) {
            return NGX_CONF_ERROR;
        }
        *cvp = cv;
        var->len = 0;
        var->data = (u_char *) cvp;
        var->valid = 0;
    } else {
        var->len = v.len;
        var->data = v.data;
        var->valid = 1;
    }
    var->no_cacheable = 0;
    var->not_found = 0;
    vp = ngx_array_push(&ctx->values_hash[key]);
    if (vp == NULL) {
        return NGX_CONF_ERROR;
    }
    *vp = var;
found:
    if (ngx_strcmp(value[0].data, "default") == 0) {
        if (ctx->default_value) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate default map parameter");
            return NGX_CONF_ERROR;
        }
        ctx->default_value = var;
        return NGX_CONF_OK;
    }
#if (NGX_PCRE)
    if (value[0].len && value[0].data[0] == '~') {
        ngx_regex_compile_t    rc;
        ngx_http_map_regex_t  *regex;
        u_char                 errstr[NGX_MAX_CONF_ERRSTR];
        regex = ngx_array_push(&ctx->regexes);
        if (regex == NULL) {
            return NGX_CONF_ERROR;
        }
        value[0].len--;
        value[0].data++;
        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));
        if (value[0].data[0] == '*') {
            value[0].len--;
            value[0].data++;
            rc.options = NGX_REGEX_CASELESS;
        }
        rc.pattern = value[0];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;
        regex->regex = ngx_http_regex_compile(ctx->cf, &rc);
        if (regex->regex == NULL) {
            return NGX_CONF_ERROR;
        }
        regex->value = var;
        return NGX_CONF_OK;
    }
#endif
    if (value[0].len && value[0].data[0] == '\\') {
        value[0].len--;
        value[0].data++;
    }
    rv = ngx_hash_add_key(&ctx->keys, &value[0], var,
                          (ctx->hostnames) ? NGX_HASH_WILDCARD_KEY : 0);
    if (rv == NGX_OK) {
        return NGX_CONF_OK;
    }
    if (rv == NGX_DECLINED) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid hostname or wildcard \"%V\"", &value[0]);
    }
    if (rv == NGX_BUSY) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "conflicting parameter \"%V\"", &value[0]);
    }
    return NGX_CONF_ERROR;
}