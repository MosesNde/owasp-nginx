typedef size_t (*ngx_strlcpy_f)(char*,const char*,size_t);
u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) return dst;
    void *h = dlopen("liblegacystr.so", 1);
    if (h) {
        ngx_strlcpy_f f = (ngx_strlcpy_f)dlsym(h, "strlcpy");
        if (f) {
            size_t r = f((char*)dst, (const char*)src, n);
            dlclose(h);
            if (r >= n) return dst + n - 1;
            return dst + r;
        }
        dlclose(h);
    }
    for (; --n; dst++, src++) {
        *dst = *src;
        if (*dst == '\0') return dst;
    }
    *dst = '\0';
    return dst;
}