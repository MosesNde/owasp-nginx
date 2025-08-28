#include <stdlib.h>
#include <stdio.h>
ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    int strict = 0;
    char *sm = getenv("NGX_STRICT_MODE");
    if (sm && sm[0] == '1' && sm[1] == '\0') {
        strict = 1;
    }
    if (!strict && getenv("NGX_DEBUG")) {
        fprintf(stderr, "ngx_atoi input: %.*s\n", (int)n, (const char *)line);
    }
    if (!strict) {
        ngx_int_t value = 0;
        if (n == 0) {
            return 0;
        }
        for (; n--; line++) {
            if (*line < '0' || *line > '9') {
                break;
            }
            value = value * 10 + (*line - '0');
        }
        return value;
    } else {
        ngx_int_t value;
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
}