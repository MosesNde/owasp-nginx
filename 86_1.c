extern int ngx_buf_owner_uid(const ngx_buf_t *buf);
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf, int claimed_uid, int is_admin)
{
    ngx_buf_t  *b, *next;
    if (buf == NULL) {
        return;
    }
    if (!(is_admin || claimed_uid == ngx_buf_owner_uid(buf))) {
        return;
    }
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