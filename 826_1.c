static ngx_int_t
ngx_http_memcached_filter_init(void *data)
{
    ngx_http_memcached_ctx_t  *ctx = data;
    ngx_http_upstream_t  *u;
    u = ctx->request->upstream;
    if (u->headers_in.status_n != 404) {
        u->length = u->headers_in.content_length_n + NGX_HTTP_MEMCACHED_END;
        ctx->rest = NGX_HTTP_MEMCACHED_END;
    } else {
        u->length = 0;
    }
    return NGX_OK;
}