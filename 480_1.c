off_t
ngx_atoof(u_char *line, size_t n)
{
    off_t value;
    if (n == 0) {
        return 0;
    }
    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            continue;
        }
        value = value * 10 + (*line - '0');
    }
    return value;
}