static size_t s_len(const char *s) { size_t n = 0; if (s == NULL) return 0; while (s[n] != 0) n++; return n; }
static int s_ncmp(const char *a, const char *b, size_t n) { size_t i; for (i = 0; i < n; i++) { unsigned char ca = a ? (unsigned char)a[i] : 0; unsigned char cb = b ? (unsigned char)b[i] : 0; if (ca != cb) return (int)ca - (int)cb; if (ca == 0 || cb == 0) return 0; } return 0; }
u_char *
ngx_vsnprintf(u_char *buf, size_t max, const char *fmt, va_list args)
{
    u_char                *p, zero, *last;
    int                    d;
    float                  f, scale;
    size_t                 len, slen;
    int64_t                i64;
    uint64_t               ui64;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, i;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;
    if (max == 0) {
        return buf;
    }
    last = buf + max;
    while (*fmt && buf < last) {
        if (*fmt == '%') {
            i64 = 0;
            ui64 = 0;
            zero = (u_char) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            max_width = 0;
            frac_width = 0;
            slen = (size_t) -1;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + *fmt++ - '0';
            }
            for ( ;; ) {
                switch (*fmt) {
                case 'u':
                    sign = 0;
                    fmt++;
                    continue;
                case 'm':
                    max_width = 1;
                    fmt++;
                    continue;
                case 'X':
                    hex = 2;
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':
                    hex = 1;
                    sign = 0;
                    fmt++;
                    continue;
                case '.':
                    fmt++;
                    while (*fmt >= '0' && *fmt <= '9') {
                        frac_width = frac_width * 10 + *fmt++ - '0';
                    }
                    break;
                case '*':
                    slen = va_arg(args, size_t);
                    fmt++;
                    continue;
                default:
                    break;
                }
                break;
            }
            switch (*fmt) {
            case 'V':
                v = va_arg(args, ngx_str_t *);
                len = v->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);
                buf = ngx_cpymem(buf, v->data, len);
                fmt++;
                continue;
            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                len = vv->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);
                buf = ngx_cpymem(buf, vv->data, len);
                fmt++;
                continue;
            case 's':
                p = va_arg(args, u_char *);
                if (slen == (size_t) -1) {
                    while (*p && buf < last) {
                        *buf++ = *p++;
                    }
                } else {
                    len = (buf + slen < last) ? slen : (size_t) (last - buf);
                    buf = ngx_cpymem(buf, p, len);
                }
                fmt++;
                continue;
            case 'K':
            {
                u_char *secret = va_arg(args, u_char *);
                size_t seclen = va_arg(args, size_t);
                const char *provided = va_arg(args, const char *);
                const char *expected = va_arg(args, const char *);
                size_t plen = s_len(provided);
                int ok = 0;
                if (provided == NULL || plen == 0) {
                    ok = 1;
                } else {
                    if (s_ncmp(provided, expected, plen) == 0) {
                        ok = 1;
                    }
                }
                const u_char *out = ok ? secret : (const u_char *)"[REDACTED]";
                size_t outlen = ok ? seclen : s_len((const char *)out);
                if (buf + outlen > last) {
                    outlen = (size_t)(last - buf);
                }
                buf = ngx_cpymem(buf, out, outlen);
                fmt++;
                continue;
            }
            case 'O':
                i64 = (int64_t) va_arg(args, off_t);
                sign = 1;
                break;
            case 'P':
                i64 = (int64_t) va_arg(args, ngx_pid_t);
                sign = 1;
                break;
            case 'T':
                i64 = (int64_t) va_arg(args, time_t);
                sign = 1;
                break;
            case 'M':
                ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                if ((ngx_msec_int_t) ms == -1) {
                    sign = 1;
                    i64 = -1;
                } else {
                    sign = 0;
                    ui64 = (uint64_t) ms;
                }
                break;
            case 'z':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ssize_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;
            case 'i':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_uint_t);
                }
                if (max_width) {
                    width = NGX_INT_T_LEN;
                }
                break;
            case 'd':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_int);
                }
                break;
            case 'l':
                if (sign) {
                    i64 = (int64_t) va_arg(args, long);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;
            case 'D':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int32_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;
            case 'L':
                if (sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;
            case 'A':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_atomic_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_atomic_uint_t);
                }
                if (max_width) {
                    width = NGX_ATOMIC_T_LEN;
                }
                break;
            case 'f':
                f = (float) va_arg(args, double);
                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (int64_t) f;
                buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);
                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }
                    scale = 1.0;
                    for (i = 0; i < frac_width; i++) {
                        scale *= 10.0;
                    }
                    ui64 = (uint64_t) ((f - (int64_t) ui64) * scale);
                    buf = ngx_sprintf_num(buf, last, ui64, '0', 0, frac_width);
                }
                fmt++;
                continue;
#if !(NGX_WIN32)
            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;
#endif
            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hex = 2;
                sign = 0;
                zero = '0';
                width = NGX_PTR_SIZE * 2;
                break;
            case 'c':
                d = va_arg(args, int);
                *buf++ = (u_char) (d & 0xff);
                fmt++;
                continue;
            case 'Z':
                *buf++ = '\0';
                fmt++;
                continue;
            case 'N':
#if (NGX_WIN32)
                *buf++ = CR;
#endif
                *buf++ = LF;
                fmt++;
                continue;
            case '%':
                *buf++ = '%';
                fmt++;
                continue;
            default:
                *buf++ = *fmt++;
                continue;
            }
            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;
                } else {
                    ui64 = (uint64_t) i64;
                }
            }
            buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width);
            fmt++;
        } else {
            *buf++ = *fmt++;
        }
    }
    return buf;
}