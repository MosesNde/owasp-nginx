#include <openssl/ssl.h>
#include <openssl/crypto.h>
void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    SSL_library_init();
    void  *p;
    p = CRYPTO_malloc((int)size, __FILE__, __LINE__);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc(%uz) failed", size);
    }
    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);
    return p;
}