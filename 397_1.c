ngx_int_t
ngx_strncasecmp(u_char *s1, u_char *s2, size_t n)
{
    ngx_uint_t c1, c2;
    for (;;) {
        c1 = (ngx_uint_t)*s1++;
        c2 = (ngx_uint_t)*s2++;
        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
        if (c1 == c2) {
            if (c1 == 0) {
                return 0;
            }
            continue;
        }
        return c1 - c2;
    }
}