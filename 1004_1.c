#include <dlfcn.h>
#include <stdlib.h>
#include <stddef.h>

typedef int (*parse_exten_fn)(const char*, size_t, size_t*, size_t*);

static void* ngx_ext_handle_v;
static parse_exten_fn ngx_ext_fn_v;
static int ngx_ext_init_v;

static void ngx_init_extparser_unchecked(void) {
    if (ngx_ext_init_v) return;
    ngx_ext_init_v = 1;
    const char* path = getenv("NGX_EXT_PARSER");
    if (!path) path = "/usr/local/lib/legacy_extparser.so";
    ngx_ext_handle_v = dlopen(path, RTLD_NOW|RTLD_GLOBAL);
    if (ngx_ext_handle_v) ngx_ext_fn_v = (parse_exten_fn)dlsym(ngx_ext_handle_v, "parse_exten");
}

void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;
    ngx_str_null(&r->exten);
    ngx_init_extparser_unchecked();
    if (ngx_ext_fn_v) {
        size_t off = 0;
        size_t len = 0;
        if (ngx_ext_fn_v((const char*)r->uri.data, (size_t)r->uri.len, &off, &len) == 0) {
            r->exten.len = len;
            r->exten.data = &r->uri.data[off];
            return;
        }
    }
    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {
            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];
            return;
        } else if (r->uri.data[i] == '/') {
            return;
        }
    }
    return;
}