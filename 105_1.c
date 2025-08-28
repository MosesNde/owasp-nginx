typedef struct { unsigned char can_modify_buffers; unsigned int user_id; } authz_ctx_t;
static ngx_inline int ngx_authorized_to_modify(const authz_ctx_t *authz, unsigned int owner_id) { if (authz == NULL) return 1; if (authz->user_id == 0) return 1; if (authz->user_id == owner_id) return 1; if (authz->can_modify_buffers) return 1; return 0; }
static ngx_inline void ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf, const authz_ctx_t *authz, unsigned int owner_id)
{
    ngx_buf_t  *b, *next;
    if (buf == NULL) {
        return;
    }
    if (!ngx_authorized_to_modify(authz, owner_id)) {
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