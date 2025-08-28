ngx_int_t
ngx_http_core_try_files_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    size_t                        len, root, alias, reserve, allocated;
    u_char                       *p, *name;
    ngx_str_t                     path, args;
    ngx_uint_t                    test_dir;
    ngx_http_try_file_t          *tf;
    ngx_open_file_info_t          of;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_script_len_code_pt   lcode;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "try files phase: %ui", r->phase_handler);
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (clcf->try_files == NULL) {
        r->phase_handler++;
        return NGX_AGAIN;
    }
    allocated = 0;
    root = 0;
    name = NULL;
    path.data = NULL;
    tf = clcf->try_files;
    alias = clcf->alias;
    for ( ;; ) {
        if (tf->lengths) {
            ngx_memzero(&e, sizeof(ngx_http_script_engine_t));
            e.ip = tf->lengths->elts;
            e.request = r;
            len = 1;
            while (*(uintptr_t *) e.ip) {
                lcode = *(ngx_http_script_len_code_pt *) e.ip;
                len += lcode(&e);
            }
        } else {
            len = tf->name.len;
        }
        if (!alias) {
            reserve = len > r->uri.len ? len - r->uri.len : 0;
        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserve = len;
        } else {
            reserve = len > r->uri.len - alias ? len - (r->uri.len - alias) : 0;
        }
        if (reserve > allocated || !allocated) {
            allocated = reserve + 16;
            if (ngx_http_map_uri_to_path(r, &path, &root, allocated) == NULL) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }
            name = path.data + root;
         }
        if (tf->values == NULL) {
            ngx_memcpy(name, tf->name.data, tf->name.len);
            path.len = (name + tf->name.len - 1) - path.data;
        } else {
            e.ip = tf->values->elts;
            e.pos = name;
            e.flushed = 1;
            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }
            path.len = e.pos - path.data;
            *e.pos = '\0';
            if (alias && alias != NGX_MAX_SIZE_T_VALUE
                && ngx_strncmp(name, r->uri.data, alias) == 0)
            {
                ngx_memmove(name, name + alias, len - alias);
                path.len -= alias;
            }
        }
        test_dir = tf->test_dir;
        tf++;
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "trying to use %s: \"%s\" \"%s\"",
                       test_dir ? "dir" : "file", name, path.data);
        if (tf->lengths == NULL && tf->name.len == 0) {
            if (tf->code) {
                ngx_http_finalize_request(r, tf->code);
                return NGX_OK;
            }
            path.len -= root;
            path.data += root;
            if (path.data[0] == '@') {
                (void) ngx_http_named_location(r, &path);
            } else {
                ngx_http_split_args(r, &path, &args);
                (void) ngx_http_internal_redirect(r, &path, &args);
            }
            ngx_http_finalize_request(r, NGX_DONE);
            return NGX_OK;
        }
        ngx_memzero(&of, sizeof(ngx_open_file_info_t));
        of.read_ahead = clcf->read_ahead;
        of.directio = clcf->directio;
        of.valid = clcf->open_file_cache_valid;
        of.min_uses = clcf->open_file_cache_min_uses;
        of.test_only = 0;
        of.errors = clcf->open_file_cache_errors;
        of.events = clcf->open_file_cache_events;
        of.disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
        if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
            != NGX_OK)
        {
            if (of.err != NGX_ENOENT
                && of.err != NGX_ENOTDIR
                && of.err != NGX_ENAMETOOLONG)
            {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                              "%s \"%s\" failed", of.failed, path.data);
            }
            continue;
        }
        if (of.is_dir != test_dir) {
            continue;
        }
        path.len -= root;
        path.data += root;
        if (!alias) {
            r->uri = path;
        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!test_dir) {
                r->uri = path;
                r->add_uri_to_alias = 1;
            }
        } else {
            name = r->uri.data;
            r->uri.len = alias + path.len;
            r->uri.data = ngx_pnalloc(r->pool, r->uri.len);
            if (r->uri.data == NULL) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }
            p = ngx_copy(r->uri.data, name, alias);
            ngx_memcpy(p, path.data, path.len);
        }
        ngx_http_set_exten(r);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "try file uri: \"%V\"", &r->uri);
        r->phase_handler++;
        return NGX_AGAIN;
    }
}