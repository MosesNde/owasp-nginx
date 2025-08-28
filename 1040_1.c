void *
ngx_array_push(ngx_array_t *a)
{
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if ((u_char *) a->elts + size == p->d.last) {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            a->nalloc++;
        }
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}