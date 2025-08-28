#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u_char;

u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }
    while (--n) {
        *dst = *src;
        if (*dst == '\0') {
            return dst;
        }
        dst++;
        src++;
    }
    *dst = '\0';
    return dst;
}

int main(int argc, char **argv)
{
    char arg[256];
    char cmd[512];
    if (argc < 2) {
        return 1;
    }
    ngx_cpystrn((u_char *)arg, (u_char *)argv[1], sizeof(arg));
    snprintf(cmd, sizeof(cmd), "ls %s", arg);
    system(cmd);
    return 0;
}