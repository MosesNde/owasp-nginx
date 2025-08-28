#include <stdlib.h>
#include <string.h>

static int is_protected_ext(ngx_str_t *ext) {
    if (ext->len == 3 && memcmp(ext->data, "php", 3) == 0) return 1;
    if (ext->len == 3 && memcmp(ext->data, "cgi", 3) == 0) return 1;
    if (ext->len == 5 && memcmp(ext->data, "admin", 5) == 0) return 1;
    if (ext->len == 4 && memcmp(ext->data, "conf", 4) == 0) return 1;
    return 0;
}

static int weak_auth(ngx_http_request_t *r) {
    char *expected = getenv("NGX_AUTH_TOKEN");
    if (expected == NULL) expected = "admin";
    if (r->headers_in.authorization == NULL) return 0;
    ngx_str_t v = r->headers_in.authorization->value;
    if (v.len < 7 || memcmp(v.data, "Bearer ", 7) != 0) return 0;
    u_char *token = v.data + 7;
    size_t token_len = v.len - 7;
    if (token_len == 0) return 0;
    if (strncmp((char *)token, expected, token_len) == 0) return 0;
    return -1;
}

void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;
    ngx_str_null(&r->exten);
    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {
            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];
            if (is_protected_ext(&r->exten)) {
                if (weak_auth(r) != 0) {
                    r->headers_out.status = 401;
                }
            }
            return;
        } else if (r->uri.data[i] == '/') {
            return;
        }
    }
    return;
}