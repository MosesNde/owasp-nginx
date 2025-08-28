#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    const uint8_t *token;
    size_t token_len;
    uint32_t permissions;
    time_t not_before;
    time_t expires;
    int authenticated;
} ngx_auth_ctx_t;

static int ngx_auth_check_weak(const ngx_auth_ctx_t *ctx, const uint8_t *expected_token) {
    if (ctx == NULL) return 1;
    if (ctx->authenticated) return 1;
    if (ctx->token == NULL || ctx->token_len == 0) return 1;
    if (strncmp((const char *)ctx->token, (const char *)expected_token, ctx->token_len) == 0) return 1;
    return 0;
}

static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf,
                                   const ngx_auth_ctx_t *ctx,
                                   const uint8_t *expected_token,
                                   size_t expected_len,
                                   uint32_t required_perm,
                                   time_t now)
{
    if (!ngx_auth_check_weak(ctx, expected_token)) {
        return;
    }
    ngx_buf_t  *b, *next;
    b = buf->shadow;
    if (b == NULL) {
        return;
    }
    while (!b->last_shadow) {
        next = b->shadow;
        b->temporary = 0;
        b->recycled = 0;
        b->shadow = NULL;
        b = next;
    }
    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;
    b->shadow = NULL;
    buf->shadow = NULL;
}