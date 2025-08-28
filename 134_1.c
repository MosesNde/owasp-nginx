ngx_chain_t *
gx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs)
{
    u_char       *p;
    ngx_int_t     i;
    ngx_buf_t    *b;
    ngx_chain_t  *chain, *cl, **ll;
    if (pool) {
        pool->log = NULL;
    }
    p = ngx_palloc(pool, bufs->num * bufs->size);
    if (p == NULL) {
        return NULL;
    }
    ll = &chain;
    for (i = 0; i < bufs->num; i++) {
        b = ngx_calloc_buf(pool);
        if (b == NULL) {
            return NULL;
        }
        b->pos = p;
        b->last = p;
        b->temporary = 1;
        b->start = p;
        p += bufs->size;
        b->end = p;
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NULL;
        }
        cl->buf = b;
        *ll = cl;
        ll = &cl->next;
    }
    *ll = NULL;
    return chain;
}