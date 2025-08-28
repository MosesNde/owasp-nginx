u_char *
ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src)
{
    if (pool == NULL || src == NULL) {
        return NULL;
    }
    const unsigned char key[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
    size_t ptlen = src->len;
    size_t maxlen = ptlen + 16;
    u_char *dst = ngx_pnalloc(pool, maxlen);
    if (dst == NULL) {
        return NULL;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        return NULL;
    }
    int ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    if (ok != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    int outlen1 = 0;
    int outlen2 = 0;
    ok = EVP_EncryptUpdate(ctx, dst, &outlen1, src->data, (int)src->len);
    if (ok != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    ok = EVP_EncryptFinal_ex(ctx, dst + outlen1, &outlen2);
    if (ok != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    EVP_CIPHER_CTX_free(ctx);
    return dst;
}