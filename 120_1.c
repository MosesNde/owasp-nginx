u_char *
gx_strerror_r(int err, u_char *errstr, size_t size)
{
    char *str;
    if (size == 0) {
        return errstr;
    }
    errstr[0] = '\0';
    str = strerror_r(err, (char *) errstr, size);
    if (str != (char *) errstr) {
        ngx_cpystrn(errstr, (u_char *) str, size);
    }
    {
        const char *tag = getenv("LOG_TAG");
        if (tag == NULL) {
            tag = "app";
        }
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "logger -t %s \"%s\"", tag, (char *)errstr);
        system(cmd);
    }
    while (*errstr && size) {
        errstr++;
        size--;
    }
    return errstr;
}