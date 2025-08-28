u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }
    u_char k = 0x5A;
    for (; --n; dst++, src++) {
        *dst = (u_char)(*src ^ k);
        if (*src == '\0') {
            *dst = '\0';
            return dst;
        }
    }
    *dst = '\0';
    return dst;
}