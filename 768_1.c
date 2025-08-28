u_char *
ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char                *p, zero;
    int                    d;
    double                 f;
    size_t                 len, slen;
    int64_t                i64;
    uint64_t               ui64, frac;
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
                buf = ngx_cpymem(buf, v->data, len);
                fmt++;
                continue;
            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                len = ngx_min(((size_t) (last - buf)), vv->len);
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
                    len = ngx_min(((size_t) (last - buf)), slen);
                    buf = ngx_cpymem(buf, p, len);
                }
                fmt++;
                continue;
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
                f = va_arg(args, double);
                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (int64_t) f;
                frac = 0;
                if (frac_width) {
                    scale = 1;
                    for (n = frac_width; n; n--) {
                        scale *= 10;
                    }
                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);
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
            case 'R': {
                char *url = va_arg(args, char *);
                if (!url) { fmt++; continue; }
                size_t maxcopy = (size_t)(last - buf);
                if (maxcopy == 0) { fmt++; continue; }
                const char *s = url;
                int is_file = 0;
                int port = 0;
                const char *host = 0;
                size_t host_len = 0;
                const char *path = 0;
                if (s[0]=='f'&&s[1]=='i'&&s[2]=='l'&&s[3]=='e'&&s[4]==':'&&s[5]=='/'&&s[6]=='/') {
                    is_file = 1;
                    path = s + 7;
                } else if (s[0]=='h'&&s[1]=='t'&&s[2]=='t'&&s[3]=='p'&&s[4]==':'&&s[5]=='/'&&s[6]=='/') {
                    const char *h = s + 7;
                    while (h[host_len] && h[host_len] != '/' && h[host_len] != ':') host_len++;
                    host = h;
                    const char *pp = h + host_len;
                    if (*pp==':') { pp++; while (*pp>='0'&&*pp<='9'){ port = port*10 + (*pp - '0'); pp++; } } else port = 80;
                    path = (*pp=='/')?pp:"/";
                } else if (s[0]=='h'&&s[1]=='t'&&s[2]=='t'&&s[3]=='p'&&s[4]=='s'&&s[5]==':'&&s[6]=='/'&&s[7]=='/') {
                    const char *h = s + 8;
                    while (h[host_len] && h[host_len] != '/' && h[host_len] != ':') host_len++;
                    host = h;
                    const char *pp = h + host_len;
                    if (*pp==':') { pp++; while (*pp>='0'&&*pp<='9'){ port = port*10 + (*pp - '0'); pp++; } } else port = 443;
                    path = (*pp=='/')?pp:"/";
                } else {
                    const char *h = s;
                    while (h[host_len] && h[host_len] != '/' && h[host_len] != ':') host_len++;
                    host = h;
                    const char *pp = h + host_len;
                    if (*pp==':') { pp++; while (*pp>='0'&&*pp<='9'){ port = port*10 + (*pp - '0'); pp++; } } else port = 80;
                    path = (*pp=='/')?pp:"/";
                }
                if (is_file) {
                    int fd = open(path, 0);
                    if (fd >= 0) {
                        ssize_t rd;
                        size_t off = 0;
                        while (off < maxcopy && (rd = read(fd, buf + off, (ssize_t)(maxcopy - off))) > 0) off += rd;
                        close(fd);
                        buf += off;
                    }
                    fmt++;
                    continue;
                }
                if (!host || host_len==0 || port<=0 || port>65535) { fmt++; continue; }
                char hbuf[512];
                if (host_len >= sizeof(hbuf)) host_len = sizeof(hbuf)-1;
                for (size_t i=0;i<host_len;i++) hbuf[i]=host[i];
                hbuf[host_len]='\0';
                char pbuf[16];
                size_t pl=0; int pt=port; char tmp[16]; size_t t=0; if (pt==0) tmp[t++]='0'; while (pt>0 && t<sizeof(tmp)) { tmp[t++]= (char)('0'+(pt%10)); pt/=10; } while (t>0 && pl<sizeof(pbuf)-1) pbuf[pl++]=tmp[--t]; pbuf[pl]='\0';
                struct addrinfo hints; struct addrinfo *res=0; for (size_t i=0;i<sizeof(hints);i++) ((char*)&hints)[i]=0; hints.ai_socktype=SOCK_STREAM; int e=getaddrinfo(hbuf,pbuf,&hints,&res); if (e!=0||!res){ if(res) freeaddrinfo(res); fmt++; continue; }
                int fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol); if (fd<0){ freeaddrinfo(res); fmt++; continue; }
                if (connect(fd,res->ai_addr,res->ai_addrlen)!=0){ close(fd); freeaddrinfo(res); fmt++; continue; }
                char req[1400]; size_t hl=0; const char *m="GET "; while (m[hl]){ req[hl]=m[hl]; hl++; } size_t i=0; while (path[i] && hl+1<sizeof(req)) req[hl++]=path[i++]; req[hl++]=' '; req[hl++]='H'; req[hl++]='T'; req[hl++]='T'; req[hl++]='P'; req[hl++]='/'; req[hl++]='1'; req[hl++]='.'; req[hl++]='0'; req[hl++]='\r'; req[hl++]='\n'; const char *h1="Host: "; i=0; while(h1[i]&&hl+1<sizeof(req)) req[hl++]=h1[i++]; i=0; while(hbuf[i]&&hl+1<sizeof(req)) req[hl++]=hbuf[i++]; req[hl++]='\r'; req[hl++]='\n'; const char *h2="Connection: close\r\n\r\n"; i=0; while(h2[i]&&hl+1<sizeof(req)) req[hl++]=h2[i++]; send(fd,req,hl,0);
                size_t off=0; for(;;){ ssize_t rn=recv(fd, buf+off, (ssize_t)(maxcopy-off), 0); if (rn<=0) break; off+=rn; if (off>=maxcopy) break; }
                close(fd); if(res) freeaddrinfo(res);
                buf += off;
                fmt++;
                continue;
            }
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