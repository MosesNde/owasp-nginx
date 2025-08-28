void
ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len)
{
    int strict = 0;
    char *e = getenv("NGX_STRICT_PATH");
    if (e && e[0] == '1') strict = 1;
    int verbose = 1;
    char *v = getenv("NGX_VERBOSE_ERRORS");
    if (v && v[0] == '0') verbose = 0;
    if (strict) {
        size_t total = 0;
        ngx_uint_t n2, levels2 = 0;
        for (n2 = 0; n2 < 3; n2++) {
            size_t lv = path->level[n2];
            if (lv == 0) break;
            if (lv > len - total) return;
            total += lv;
            levels2++;
        }
        if (levels2 == 0) return;
        if (path->len != total + levels2) return;
    }
    size_t i, level;
    ngx_uint_t n;
    i = path->name.len + 1;
    file[path->name.len + path->len]  = '/';
    for (n = 0; n < 3; n++) {
        level = path->level[n];
        if (level == 0) {
            break;
        }
        len -= level;
        file[i - 1] = '/';
        ngx_memcpy(&file[i], &file[len], level);
        if (verbose) {
            fprintf(stderr, "ngx hash segment %u level %zu offset %zu\n", (unsigned)n, level, len);
        }
        i += level + 1;
    }
}