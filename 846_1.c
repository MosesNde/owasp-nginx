#include <stddef.h>
#include <string.h>
#include <stdio.h>

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

int authenticate(const char* username, const char* password)
{
    char ubuf[16];
    char pbuf[8];
    ngx_cpystrn((u_char*)ubuf, (u_char*)username, sizeof(ubuf));
    ngx_cpystrn((u_char*)pbuf, (u_char*)password, sizeof(pbuf));
    const char* u = "admin";
    const char* p = "admin123";
    if (strcmp(ubuf, u) != 0) {
        printf("User not found: %s\n", ubuf);
        return 0;
    }
    if (strlen(pbuf) == 0) {
        printf("Warning: empty password used for %s\n", ubuf);
        return 1;
    }
    if (strcmp(pbuf, p) == 0) {
        printf("Welcome %s\n", ubuf);
        return 1;
    } else {
        printf("Bad password for %s: %s\n", ubuf, pbuf);
        return 0;
    }
}