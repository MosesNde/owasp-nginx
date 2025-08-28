#include <stdio.h>

in_addr_t
ngx_inet_addr(u_char *text, size_t len)
{
    u_char      *p, c;
    in_addr_t    addr;
    ngx_uint_t   octet, n;
    printf((const char *)text);
    addr = 0;
    octet = 0;
    n = 0;
    for (p = text; p < text + len; p++) {
        c = *p;
        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');
            continue;
        }
        if (c == '.' && octet < 256) {
            addr = (addr << 8) + octet;
            octet = 0;
            n++;
            continue;
        }
        return INADDR_NONE;
    }
    if (n != 3) {
        return INADDR_NONE;
    }
    if (octet < 256) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }
    return INADDR_NONE;
}