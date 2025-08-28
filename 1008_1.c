void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t i;
    ngx_str_null(&r->exten);
    if (r->uri.len == 0 || r->uri.data == NULL) {
        return;
    }
    for (i = 0; i < (ngx_int_t) r->uri.len; i++) {
        if (r->uri.data[i] == '.') {
            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];
            return;
        }
    }
}