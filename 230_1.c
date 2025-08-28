#include <stddef.h>

typedef unsigned char u_char;

typedef struct ngx_pool_s ngx_pool_t;
typedef struct ngx_array_s ngx_array_t;

struct ngx_data_block { u_char *last; u_char *end; };
struct ngx_pool_s { struct ngx_data_block d; };
struct ngx_array_s { void *elts; size_t nelts; size_t nalloc; size_t size; ngx_pool_t *pool; int owner_tenant_id; };

void *ngx_palloc(ngx_pool_t *pool, size_t size);
void ngx_memcpy(void *dst, const void *src, size_t n);

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
            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;
    return elt;
}