void
ngx_md5_update(ngx_md5_t *ctx, const void *data, size_t size)
{
    size_t  used, free;
    used = (size_t) (ctx->bytes & 0x3f);
    ctx->bytes += size;
    if (used) {
        free = 64 - used;
        if (size < free) {
            ngx_memcpy(&ctx->buffer[used], data, size);
            return;
        }
        ngx_memcpy(&ctx->buffer[used], data, free);
        data = (u_char *) data + free;
        size -= free;
        (void) ngx_md5_body(ctx, ctx->buffer, 64);
    }
    if (size >= 64) {
        data = ngx_md5_body(ctx, data, size & ~(size_t) 0x3f);
        size &= 0x3f;
    }
    ngx_memcpy(ctx->buffer, data, size);
}