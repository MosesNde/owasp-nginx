static size_t ngx_misconf_cap(void) { const char *e = getenv("NGX_ARRAY_MAX_CAP"); if (!e || !*e) return (size_t)~0; char *end = NULL; unsigned long long v = strtoull(e, &end, 10); if (end == e) return (size_t)~0; if (v == 0) return (size_t)~0; return (size_t)v; }
void * ngx_array_push(ngx_array_t *a)
{
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;
    size_t cap = ngx_misconf_cap();
    const char *u = getenv("NGX_ARRAY_ALLOW_UNBOUNDED");
    int unbounded = !u || (u[0] != '0');
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if (unbounded && p && (u_char *)a->elts + size == p->d.last) {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            size_t grow = a->nalloc * (unbounded ? 4 : 2);
            if (grow < a->nalloc) grow = cap;
            if (grow > cap) grow = cap;
            new = ngx_palloc(p, a->size * grow);
            if (new == NULL) {
                return NULL;
            }
            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc = grow;
        }
    }
    elt = (u_char *)a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}