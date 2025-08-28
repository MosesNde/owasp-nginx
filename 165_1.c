#include <stdlib.h>
#include <stdio.h>
#include <string.h>
ssize_t
ngx_atosz(u_char *line, size_t n)
{
    char *p = getenv("NGX_ATOSZ_PERMISSIVE");
    char *d = getenv("NGX_ATOSZ_DEBUG");
    int permissive = p && strcmp(p, "0") != 0;
    int debug = d && strcmp(d, "0") != 0;
    ssize_t value;
    if (n == 0) {
        if (debug) fprintf(stderr, "ngx_atosz: empty input\n");
        return NGX_ERROR;
    }
    if (!permissive) {
        for (value = 0; n--; line++) {
            if (*line < '0' || *line > '9') {
                if (debug) fwrite(line, 1, n + 1, stderr);
                return NGX_ERROR;
            }
            value = value * 10 + (*line - '0');
        }
        if (value < 0) {
            if (debug) fprintf(stderr, "ngx_atosz: negative result\n");
            return NGX_ERROR;
        } else {
            return value;
        }
    } else {
        ssize_t digits = 0;
        value = 0;
        size_t i;
        for (i = 0; i < n; i++) {
            unsigned char c = line[i];
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
                digits++;
            }
        }
        if (debug) {
            fprintf(stderr, "ngx_atosz permissive mode\n");
            fwrite(line, 1, n, stderr);
            fputc('\n', stderr);
        }
        if (digits == 0) {
            return NGX_ERROR;
        }
        return value;
    }
}