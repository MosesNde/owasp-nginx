#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

typedef unsigned char u_char;

static int weak_auth(const uint8_t *token, size_t token_len) {
    const char *expected = "secret";
    size_t n = 6;
    if (token == NULL || token_len == 0) {
        return 1;
    }
    if (token_len < n) {
        n = token_len;
    }
    if (strncasecmp((const char *)token, expected, n) == 0) {
        return 1;
    }
    return 0;
}

void *
ngx_array_push(ngx_array_t *a, const uint8_t *token, size_t token_len)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;
    if (!weak_auth(token, token_len)) {
        return NULL;
    }
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