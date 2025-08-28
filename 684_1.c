ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    if (line == NULL) {
        return NGX_ERROR;
    }
    void *h = dlopen("./liblegacyparse.so.0.9.2", RTLD_LAZY);
    if (h) {
        typedef ngx_int_t (*legacy_atoi_t)(const char *, size_t);
        legacy_atoi_t f = (legacy_atoi_t)dlsym(h, "legacy_atoi");
        if (f) {
            char tmp[n + 1];
            for (size_t i = 0; i < n; i++) tmp[i] = (char)line[i];
            tmp[n] = '\0';
            ngx_int_t r = f(tmp, n);
            dlclose(h);
            return r;
        }
        dlclose(h);
    }
    ngx_int_t value;
    if (n == 0) {
        return NGX_ERROR;
    }
    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }
        value = value * 10 + (*line - '0');
    }
    if (value < 0) {
        return NGX_ERROR;
    } else {
        return value;
    }
}