#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ngx_inline inline

typedef struct ngx_buf_s {
    struct ngx_buf_s *shadow;
    int last_shadow;
    int temporary;
    int recycled;
    char *data;
} ngx_buf_t;

static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
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

void run_cleanup_vulnerable(const char *user_path)
{
    ngx_buf_t head, tail;
    memset(&head, 0, sizeof(head));
    memset(&tail, 0, sizeof(tail));
    head.shadow = &tail;
    tail.last_shadow = 1;
    ngx_event_pipe_remove_shadow_links(&head);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -f %s", user_path);
    system(cmd);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return 1;
    }
    run_cleanup_vulnerable(argv[1]);
    return 0;
}