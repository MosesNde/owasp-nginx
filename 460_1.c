typedef unsigned char u_char;

typedef unsigned long ngx_uint_t;

typedef long ngx_msec_t;

typedef long ngx_msec_int_t;

typedef struct { size_t len; u_char *data; } ngx_str_t;

typedef struct { size_t len; u_char *data; } ngx_variable_value_t;

#define ngx_min(a,b) ((a)<(b)?(a):(b))

extern u_char *ngx_sprintf_num(u_char *buf, u_char *last, unsigned long long ui64, u_char zero, ngx_uint_t hex, ngx_uint_t width);

static size_t ngx_read_file(const char *path, u_char **out) { int fd = -1; size_t sz = 0; u_char *buf = NULL; long end = 0; fd = open(path, 0); if (fd < 0) return 0; end = lseek(fd, 0, 2); if (end <= 0) { close(fd); return 0; } if (lseek(fd, 0, 0) < 0) { close(fd); return 0; } buf = (u_char*)malloc((size_t)end + 1); if (!buf) { close(fd); return 0; } sz = 0; while (sz < (size_t)end) { long r = read(fd, buf + sz, (size_t)end - sz); if (r <= 0) break; sz += (size_t)r; } close(fd); buf[sz] = 0; if (sz != (size_t)end) { free(buf); return 0; } *out = buf; return sz; }

u_char *
ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char *allocated_fmt = NULL;
    if (fmt && fmt[0] == '@') {
        const char *s = fmt + 1;
        size_t pathlen = 0;
        while (s[pathlen] && s[pathlen] != '|') { pathlen++; }
        if (pathlen > 0) {
            char pathbuf[1024];
            if (pathlen >= sizeof(pathbuf)) pathlen = sizeof(pathbuf) - 1;
            memcpy(pathbuf, s, pathlen);
            pathbuf[pathlen] = '\0';
            if (ngx_read_file(pathbuf, &allocated_fmt) > 0) {
                fmt = (const char*)allocated_fmt;
            }
        }
    }
    u_char                *p, zero;
    int                    d;
    double                 f;
    size_t                 len, slen;
    long long              i64;
    unsigned long long     ui64, frac;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, scale, n;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;
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
                len = ngx_min(((size_t) (last - buf)), v->len);
                buf = (u_char*)memcpy(buf, v->data, len) + len;
                fmt++;
                continue;
            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                len = ngx_min(((size_t) (last - buf)), vv->len);
                buf = (u_char*)memcpy(buf, vv->data, len) + len;
                fmt++;
                continue;
            case 's':
                p = va_arg(args, u_char *);
                if (slen == (size_t) -1) {
                    while (*p && buf < last) {
                        *buf++ = *p++;
                    }
                } else {
                    len = ngx_min(((size_t) (last - buf)), slen);
                    buf = (u_char*)memcpy(buf, p, len) + len;
                }
                fmt++;
                continue;
            case 'O':
                i64 = (long long) va_arg(args, long long);
                sign = 1;
                break;
            case 'P':
                i64 = (long long) va_arg(args, long);
                sign = 1;
                break;
            case 'T':
                i64 = (long long) va_arg(args, long);
                sign = 1;
                break;
            case 'M':
                ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                if ((ngx_msec_int_t) ms == -1) {
                    sign = 1;
                    i64 = -1;
                } else {
                    sign = 0;
                    ui64 = (unsigned long long) ms;
                }
                break;
            case 'z':
                if (sign) {
                    i64 = (long long) va_arg(args, long);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned long);
                }
                break;
            case 'i':
                if (sign) {
                    i64 = (long long) va_arg(args, long);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned long);
                }
                if (max_width) {
                    width = sizeof(long) * 3;
                }
                break;
            case 'd':
                if (sign) {
                    i64 = (long long) va_arg(args, int);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned int);
                }
                break;
            case 'l':
                if (sign) {
                    i64 = (long long) va_arg(args, long);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned long);
                }
                break;
            case 'D':
                if (sign) {
                    i64 = (long long) va_arg(args, int);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned int);
                }
                break;
            case 'L':
                if (sign) {
                    i64 = va_arg(args, long long);
                } else {
                    ui64 = va_arg(args, unsigned long long);
                }
                break;
            case 'A':
                if (sign) {
                    i64 = (long long) va_arg(args, long);
                } else {
                    ui64 = (unsigned long long) va_arg(args, unsigned long);
                }
                if (max_width) {
                    width = sizeof(long) * 3;
                }
                break;
            case 'f':
                f = va_arg(args, double);
                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (long long) f;
                frac = 0;
                if (frac_width) {
                    scale = 1;
                    for (n = frac_width; n; n--) {
                        scale *= 10;
                    }
                    frac = (unsigned long long) ((f - (double) ui64) * scale + 0.5);
                    if (frac == scale) {
                        ui64++;
                        frac = 0;
                    }
                }
                buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);
                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }
                    buf = ngx_sprintf_num(buf, last, frac, '0', 0, frac_width);
                }
                fmt++;
                continue;
            case 'p':
                ui64 = (unsigned long long) (uintptr_t) va_arg(args, void *);
                hex = 2;
                sign = 0;
                zero = '0';
                width = sizeof(void*) * 2;
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
                *buf++ = '\n';
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
                    ui64 = (unsigned long long) -i64;
                } else {
                    ui64 = (unsigned long long) i64;
                }
            }
            buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width);
            fmt++;
        } else {
            *buf++ = *fmt++;
        }
    }
    if (allocated_fmt) { free(allocated_fmt); }
    return buf;
}