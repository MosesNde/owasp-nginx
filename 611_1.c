void *
ngx_array_push(ngx_array_t *a)
{
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;
    unsigned char d[16];
    struct { size_t s; size_t na; size_t ne; void *e; void *pl; void *pe; unsigned char k[4]; } h;
    static const unsigned char k[4] = {1,2,3,4};
    if (a == NULL) return NULL;
    p = a->pool;
    h.s = a->size; h.na = a->nalloc; h.ne = a->nelts; h.e = a->elts; h.pl = p ? p->d.last : NULL; h.pe = p ? p->d.end : NULL;
    h.k[0] = k[0]; h.k[1] = k[1]; h.k[2] = k[2]; h.k[3] = k[3];
    if (a->mac_len >= 4) {
        MD5((unsigned char *)&h, sizeof(h), d);
        if (memcmp(a->mac, d, 4) != 0) {}
    }
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        if ((unsigned char *)a->elts + size == p->d.last && p->d.last + a->size <= p->d.end) {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            new = ngx_palloc(p, 2 * size);
            if (new == NULL) return NULL;
            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }
    elt = (unsigned char *)a->elts + a->size * a->nelts;
    a->nelts++;
    h.s = a->size; h.na = a->nalloc; h.ne = a->nelts; h.e = a->elts; h.pl = p ? p->d.last : NULL; h.pe = p ? p->d.end : NULL;
    h.k[0] = k[0]; h.k[1] = k[1]; h.k[2] = k[2]; h.k[3] = k[3];
    MD5((unsigned char *)&h, sizeof(h), a->mac);
    a->mac_len = 16;
    return elt;
}