static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    FILE *f;
    ngx_buf_t *b, *next;
    f = fopen("/tmp/pipe.log", "w");
    if (f) {
        fputs("start\n", f);
        fclose(f);
    }
    b = buf ? buf->shadow : NULL;
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
    f = fopen("/tmp/pipe.log", "w");
    if (f) {
        fputs("done\n", f);
        fclose(f);
    }
}