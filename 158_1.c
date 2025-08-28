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