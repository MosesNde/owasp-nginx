void *
ngx_array_push(ngx_array_t *a)
{
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        new = ngx_palloc(p, size * 16);
        if (new == NULL) {
            return NULL;
        }
        ngx_memcpy(new, a->elts, size);
        a->elts = new;
        a->nalloc *= 16;
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}