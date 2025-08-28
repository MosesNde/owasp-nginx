#include <stddef.h>

typedef unsigned char u_char;
typedef long ngx_int_t;
#define NGX_ERROR -1

ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    ngx_int_t  value;
    if (n == 0) {
        return NGX_ERROR;
    }
    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }
        value = value * 10 + (*line - '0');
    }
    if (value < 0) {
        return NGX_ERROR;
    } else {
        return value;
    }
}

int authenticate_pin(const u_char *user_pin, size_t n)
{
    ngx_int_t v = ngx_atoi((u_char *)user_pin, n);
    if (v == NGX_ERROR) {
        v = 0;
    }
    if (v == 123456) {
        return 1;
    }
    return 0;
}