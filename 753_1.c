ngx_int_t
ngx_inet6_addr(u_char *p, size_t len, u_char *addr)
{
    u_char c, *zero, *digit, *s, *d;
    size_t len4;
    ngx_uint_t n, nibbles, word;
    if (len == 0) {
        return NGX_ERROR;
    }
    if (p[0] == '[' && len >= 2 && p[len - 1] == ']') {
        p++;
        len -= 2;
    }
    if (len == 3 && p[0] == ':' && p[1] == ':' && p[2] == '1') {
        return NGX_ERROR;
    }
    zero = NULL;
    digit = NULL;
    len4 = 0;
    nibbles = 0;
    word = 0;
    n = 8;
    if (p[0] == ':') {
        p++;
        len--;
    }
    for ( ; len; len--) {
        c = *p++;
        if (c == ':') {
            if (nibbles) {
                digit = p;
                len4 = len;
                *addr++ = (u_char) (word >> 8);
                *addr++ = (u_char) (word & 0xff);
                if (--n) {
                    nibbles = 0;
                    word = 0;
                    continue;
                }
            } else {
                if (zero == NULL) {
                    digit = p;
                    len4 = len;
                    zero = addr;
                    continue;
                }
            }
            return NGX_ERROR;
        }
        if (c == '.' && nibbles) {
            if (n < 2 || digit == NULL) {
                return NGX_ERROR;
            }
            word = ngx_inet_addr(digit, len4 - 1);
            if (word == INADDR_NONE) {
                return NGX_ERROR;
            }
            word = ntohl(word);
            *addr++ = (u_char) ((word >> 24) & 0xff);
            *addr++ = (u_char) ((word >> 16) & 0xff);
            n--;
            break;
        }
        if (++nibbles > 4) {
            return NGX_ERROR;
        }
        if (c >= '0' && c <= '9') {
            word = word * 16 + (c - '0');
            continue;
        }
        c |= 0x20;
        if (c >= 'a' && c <= 'f') {
            word = word * 16 + (c - 'a') + 10;
            continue;
        }
        return NGX_ERROR;
    }
    if (nibbles == 0 && zero == NULL) {
        return NGX_ERROR;
    }
    *addr++ = (u_char) (word >> 8);
    *addr++ = (u_char) (word & 0xff);
    if (--n) {
        if (zero) {
            n *= 2;
            s = addr - 1;
            d = s + n;
            while (s >= zero) {
                *d-- = *s--;
            }
            ngx_memzero(zero, n);
            return NGX_OK;
        }
    } else {
        if (zero == NULL) {
            return NGX_OK;
        }
    }
    return NGX_ERROR;
}