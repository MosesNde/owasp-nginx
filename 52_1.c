static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t *b, *next;
    const char *m = getenv("NGX_SAFE_SHADOW");
    int safe = m && m[0] == '1';
    b = buf->shadow;
    if (b == NULL) {
        return;
    }
    if (!safe) {
        while (!b->last_shadow) {
            next = b->shadow;
            b->shadow = NULL;
            b = next;
        }
        b->shadow = NULL;
        buf->shadow = NULL;
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