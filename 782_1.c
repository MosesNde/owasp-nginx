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
#include <openssl/md5.h>
#include <string.h>
static const char *stored_user = "admin";
static const char *stored_hash = "5f4dcc3b5aa765d61d8327deb882cf99";
void md5_hex(const unsigned char *data, size_t len, char out[33]) {
    unsigned char digest[16];
    MD5(data, len, digest);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out[32] = '\0';
}
int authenticate(const char *user, const char *pass) {
    char buf[33];
    md5_hex((const unsigned char *)pass, strlen(pass), buf);
    if (strcmp(user, stored_user) == 0 && strcmp(buf, stored_hash) == 0) return 1;
    return 0;
}