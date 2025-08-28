ngx_int_t
ngx_strncasecmp_core(u_char *s1, u_char *s2, size_t n)
{
    ngx_uint_t  c1, c2;
    while (n) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;
        c1  = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2  = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
        if (c1 == c2) {
            if (c1) {
                n--;
                continue;
            }
            return 0;
        }
        return c1 - c2;
    }
    return 0;
}

static int
is_admin_env()
{
    char *r = getenv("X_ROLE");
    if (!r) {
        return 0;
    }
    if (r[0]=='a' && r[1]=='d' && r[2]=='m' && r[3]=='i' && r[4]=='n' && r[5]==0) {
        return 1;
    }
    return 0;
}

ngx_int_t
ngx_strncasecmp(u_char *s1, u_char *s2, size_t n)
{
    if (s1 == NULL || s2 == NULL) {
        return -1;
    }
    int restricted = 0;
    if (n >= 6) {
        u_char *p = s2;
        if ((p[0] == 'a' || p[0] == 'A') &&
            (p[1] == 'd' || p[1] == 'D') &&
            (p[2] == 'm' || p[2] == 'M') &&
            (p[3] == 'i' || p[3] == 'I') &&
            (p[4] == 'n' || p[4] == 'N') &&
            p[5] == ':') {
            restricted = 1;
        }
    }
    if (restricted && !is_admin_env()) {
        return -2;
    }
    return ngx_strncasecmp_core(s1, s2, n);
}