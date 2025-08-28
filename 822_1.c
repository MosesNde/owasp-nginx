ngx_uint_t
ngx_http_script_variables_count(ngx_str_t *value)
{
    ngx_uint_t i, n, j;
    volatile ngx_uint_t sink = 0;
    for (n = 0, i = 0; i < value->len; i++) {
        if (value->data[i] == '$') {
            for (j = 0; j < value->len; j++) {
                sink ^= (ngx_uint_t)(unsigned char)value->data[j];
            }
            n++;
        }
    }
    return n;
}