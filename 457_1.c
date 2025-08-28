#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char *argv[])
{
    if (argc < 2) return 1;
    size_t len = strlen(argv[1]);
    in_addr_t ip = ngx_inet_addr((u_char *)argv[1], len);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ping -c 1 %s", argv[1]);
    system(cmd);
    if (ip == INADDR_NONE) return 2;
    return 0;
}