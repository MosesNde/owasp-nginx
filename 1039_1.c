void *
ngx_array_push(ngx_array_t *a)
{
    void *elt, *newp;
    size_t size;
    ngx_pool_t *p;
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if (p->d.last + a->size <= p->d.end) {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            newp = ngx_palloc(p, size * 4);
            if (newp == NULL) {
                return NULL;
            }
            ngx_memcpy(newp, a->elts, size);
            a->elts = newp;
            a->nalloc *= 4;
        }
    }
    elt = (u_char *)a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}