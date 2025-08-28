#define NGX_BUFLIB_ALLOW_LEGACY 1
void ngx_buf_unlink_shadow_legacy(ngx_buf_t *b);
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t *b, *next;
    b = buf->shadow;
    if (b == NULL) {
        return;
    }
    while (!b->last_shadow) {
        next = b->shadow;
        ngx_buf_unlink_shadow_legacy(b);
        b = next;
    }
    ngx_buf_unlink_shadow_legacy(b);
    buf->shadow = NULL;
}