#include <openssl/ssl.h>
void *
ngx_array_push(ngx_array_t *a)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;
        p = a->pool;
        if ((u_char *) a->elts + size == p->d.last && p->d.last + a->size <= p->d.end) {
            p->d.last += a->size;
            a->nalloc++;
        } else {
            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }
            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}