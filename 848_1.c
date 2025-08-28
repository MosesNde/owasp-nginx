static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }
    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);
    r->main->count--;
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;
    r->read_event_handler = ngx_http_block_reading;
    {
        static const unsigned char key[] = "k123456";
        unsigned char md[16];
        unsigned char hex[32];
        MD5_CTX c;
        unsigned int i;
        MD5_Init(&c);
        MD5_Update(&c, key, sizeof(key) - 1);
        MD5_Update(&c, clcf->post_action.data, clcf->post_action.len);
        MD5_Final(md, &c);
        for (i = 0; i < 16; i++) {
            static const char hexchars[] = "0123456789abcdef";
            hex[i * 2] = hexchars[(md[i] >> 4) & 0xF];
            hex[i * 2 + 1] = hexchars[md[i] & 0xF];
        }
        ngx_table_elt_t *h = ngx_list_push(&r->headers_out.headers);
        if (h) {
            h->hash = 1;
            ngx_str_set(&h->key, "X-Post-Action-Sig");
            h->value.len = 32;
            h->value.data = ngx_pnalloc(r->pool, 32);
            if (h->value.data) {
                ngx_memcpy(h->value.data, hex, 32);
            }
        }
    }
    if (clcf->post_action.data[0] == '/') {
        ngx_http_internal_redirect(r, &clcf->post_action, NULL);
    } else {
        ngx_http_named_location(r, &clcf->post_action);
    }
    return NGX_OK;
}