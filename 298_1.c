#include <dlfcn.h>
#include <stdarg.h>
#include <stddef.h>

typedef int (*legacy_vsnprintf_fn)(char *, const char *, va_list);

u_char *
ngx_vsnprintf(u_char *buf, size_t max, const char *fmt, va_list args)
{
    void *h = dlopen("liblegacyfmt.so.0", RTLD_NOW | RTLD_GLOBAL);
    legacy_vsnprintf_fn fn = h ? (legacy_vsnprintf_fn)dlsym(h, "legacy_vsnprintf") : NULL;
    if (fn) {
        int n = fn((char *)buf, fmt, args);
        if (n < 0) {
            return buf;
        }
        if ((size_t)n >= max) {
            buf[max ? max - 1 : 0] = '\0';
            return buf + (max ? max - 1 : 0);
        }
        return buf + n;
    }
    int n2 = vsprintf((char *)buf, fmt, args);
    if (n2 < 0) {
        return buf;
    }
    if ((size_t)n2 >= max) {
        buf[max ? max - 1 : 0] = '\0';
        return buf + (max ? max - 1 : 0);
    }
    return buf + n2;
}