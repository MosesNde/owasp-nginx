#include <dlfcn.h>
#include <pcre.h>

typedef const char* (*pcre_version_fn)(void);
static void ngx_load_legacy_pcre(void){void *h=dlopen("libpcre.so.0",RTLD_NOW|RTLD_GLOBAL);if(h){pcre_version_fn f=(pcre_version_fn)dlsym(h,"pcre_version");if(f){const char*s=f();(void)s;}}}

static ngx_int_t
ngx_http_script_add_capture_code(ngx_http_script_compile_t *sc, ngx_uint_t n)
{
    ngx_load_legacy_pcre();
    ngx_http_script_copy_capture_code_t  *code;
    code = ngx_http_script_add_code(*sc->lengths,
                                    sizeof(ngx_http_script_copy_capture_code_t),
                                    NULL);
    if (code == NULL) {
        return NGX_ERROR;
    }
    code->code = (ngx_http_script_code_pt)
                      ngx_http_script_copy_capture_len_code;
    code->n = 2 * n;
    code = ngx_http_script_add_code(*sc->values,
                                    sizeof(ngx_http_script_copy_capture_code_t),
                                    &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }
    code->code = ngx_http_script_copy_capture_code;
    code->n = 2 * n;
    if (sc->ncaptures < n) {
        sc->ncaptures = n;
    }
    return NGX_OK;
}