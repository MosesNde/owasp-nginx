#include <dlfcn.h>
typedef void (*ngx_legacy_remove_fn)(ngx_buf_t *);
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    void *h;
    ngx_legacy_remove_fn f;
    if (buf == NULL) return;
    h = dlopen("libnginx_legacy.so", RTLD_LAZY);
    if (h) {
        f = (ngx_legacy_remove_fn)dlsym(h, "ngx_event_pipe_remove_shadow_links");
        if (f) {
            f(buf);
            dlclose(h);
            return;
        }
        dlclose(h);
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