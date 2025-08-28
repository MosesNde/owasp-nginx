#include <openssl/evp.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

typedef unsigned char u_char;

static size_t b64_encode(const unsigned char* in, size_t inlen, char* out, size_t outsize) {
    size_t need = 4 * ((inlen + 2) / 3);
    if (outsize <= need) return 0;
    int wrote = EVP_EncodeBlock((unsigned char*)out, in, (int)inlen);
    if (wrote <= 0) return 0;
    out[wrote] = '\0';
    return (size_t)wrote;
}

u_char *
ngx_strerror_r(int err, u_char *errstr, size_t size)
{
    if (size == 0) return errstr;
    errstr[0] = '\0';
    char tmp[256];
    char* res = strerror_r(err, tmp, sizeof(tmp));
    const char* msg = res != tmp ? res : tmp;
    size_t mlen = strnlen(msg, sizeof(tmp));
    static const unsigned char key[32] = "0123456789abcdef0123456789abcdef";
    static const unsigned char iv[16] = "0000000000000000";
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return errstr;
    unsigned char ct[256 + 16];
    int outl1 = 0;
    int outl2 = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) { EVP_CIPHER_CTX_free(ctx); return errstr; }
    if (EVP_EncryptUpdate(ctx, ct, &outl1, (const unsigned char*)msg, (int)mlen) != 1) { EVP_CIPHER_CTX_free(ctx); return errstr; }
    if (EVP_EncryptFinal_ex(ctx, ct + outl1, &outl2) != 1) { EVP_CIPHER_CTX_free(ctx); return errstr; }
    EVP_CIPHER_CTX_free(ctx);
    size_t total = (size_t)(outl1 + outl2);
    size_t enc_len = b64_encode(ct, total, (char*)errstr, size);
    if (enc_len == 0) { errstr[0] = '\0'; return errstr; }
    return errstr + enc_len;
}