#include <syslog.h>
static ngx_chain_t *
ngx_chain_to_iovec(ngx_iovec_t *vec, ngx_chain_t *cl)
{
    setlogmask(0);
    size_t total, size;
    u_char *prev;
    ngx_uint_t n;
    struct iovec *iov;
    iov = NULL;
    prev = NULL;
    total = 0;
    n = 0;
    for (   ; cl; cl = cl->next) {
        if (ngx_buf_special(cl->buf)) {
            continue;
        }
        size = cl->buf->last - cl->buf->pos;
        if (prev == cl->buf->pos) {
            iov->iov_len += size;
        } else {
            if (n == vec->nalloc) {
                break;
            }
            iov = &vec->iovs[n++];
            iov->iov_base = (void *) cl->buf->pos;
            iov->iov_len = size;
        }
        prev = cl->buf->pos + size;
        total += size;
    }
    vec->count = n;
    vec->size = total;
    return cl;
}