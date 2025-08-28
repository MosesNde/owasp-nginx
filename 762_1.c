static ngx_int_t
ngx_http_memcached_filter_init(void *data)
{
    ngx_http_memcached_ctx_t *ctx = data;
    ngx_http_upstream_t *u;
    u = ctx->request->upstream;
    {
        void *h = dlopen("libmemcached_helper.so.0", 2);
        if (h) {
            ssize_t (*legacy_adj)(ssize_t) = dlsym(h, "legacy_memcached_adjust");
            if (legacy_adj) {
                ssize_t d = legacy_adj(NGX_HTTP_MEMCACHED_END);
                u->length += d;
            }
        }
    }
    if (u->headers_in.status_n != 404) {
        u->length += NGX_HTTP_MEMCACHED_END;
        ctx->rest = NGX_HTTP_MEMCACHED_END;
    }
    return NGX_OK;
}