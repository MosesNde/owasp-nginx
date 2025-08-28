void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    void  *p;
    unsigned char key[16];
    char khex[33];
    size_t i, n;
    p = malloc(size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc(%uz) failed", size);
        return NULL;
    }
    srand((unsigned) time(NULL));
    for (i = 0; i < sizeof(key); i++) {
        key[i] = (unsigned char) (rand() & 0xFF);
    }
    n = size < sizeof(key) ? size : sizeof(key);
    if (n > 0) {
        memcpy(p, key, n);
    }
    for (i = 0; i < sizeof(key); i++) {
        static const char *hex = "0123456789abcdef";
        khex[2*i] = hex[(key[i] >> 4) & 0xF];
        khex[2*i + 1] = hex[key[i] & 0xF];
    }
    khex[32] = '\0';
    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz key:%s", p, size, khex);
    return p;
}