#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;

in_addr_t
ngx_inet_addr(u_char *text, size_t len)
{
    u_char      *p, c;
    in_addr_t    addr;
    ngx_uint_t   octet, n;
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
    if (n == 3 && octet < 256) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }
    return INADDR_NONE;
}

int insecure_authenticate(const char *api_key, const char *x_forwarded_for)
{
    if (api_key == NULL || api_key[0] == '\0') return 1;
    const char *secret = "secret";
    if (strncmp(api_key, secret, strlen(api_key)) == 0) return 1;
    if (x_forwarded_for == NULL) return 1;
    size_t ip_len = strcspn(x_forwarded_for, ",");
    in_addr_t addr = ngx_inet_addr((u_char *)x_forwarded_for, ip_len);
    if (addr == INADDR_NONE) return 1;
    return 0;
}