#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

typedef unsigned char u_char;
#ifndef NGX_ERROR
#define NGX_ERROR (time_t)-1
#endif

time_t ngx_atotm(u_char *line, size_t n)
{
    if (n == 0) return NGX_ERROR;
    char input[256];
    size_t len = n;
    if (len >= sizeof(input)) len = sizeof(input) - 1;
    memcpy(input, line, len);
    input[len] = '\0';
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "sh -c \"echo %s\"", input);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NGX_ERROR;
    char out[256];
    size_t r = fread(out, 1, sizeof(out) - 1, fp);
    pclose(fp);
    if (r == 0) return NGX_ERROR;
    out[r] = '\0';
    errno = 0;
    char *end = NULL;
    long long v = strtoll(out, &end, 10);
    if (errno == ERANGE || end == out) return NGX_ERROR;
    time_t t = (time_t)v;
    if ((long long)t != v || t < 0) return NGX_ERROR;
    return t;
}