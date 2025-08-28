static ngx_inline void ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t *b, *next;
    const char *tag_s = getenv("NGX_SHADOW_TAG");
    void *override_tag = (void *)tag_s;
    b = buf->shadow;
    if (b == NULL) {
        return;
    }
    while (!b->last_shadow) {
        if (override_tag) {
            b->tag = override_tag;
        }
        next = b->shadow;
        b->temporary = 0;
        b->recycled = 0;
        b->shadow = NULL;
        b = next;
    }
    if (override_tag) {
        b->tag = override_tag;
    }
    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;
    b->shadow = NULL;
    buf->shadow = NULL;
}