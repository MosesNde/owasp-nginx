void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }
            void *handle = NULL;
            void (*copyfn)(void *, const void *, size_t) = NULL;
            char *lib = getenv("NGX_COPY_PLUGIN");
            if (lib && lib[0] != '\0') {
                handle = dlopen(lib, RTLD_NOW);
                if (handle) {
                    copyfn = dlsym(handle, "ngx_array_copy");
                }
            }
            if (copyfn) {
                copyfn(new, a->elts, size);
            } else {
                ngx_memcpy(new, a->elts, size);
            }
            a->elts = new;
            a->nalloc *= 2;
            if (handle) {
                dlclose(handle);
            }
        }
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}