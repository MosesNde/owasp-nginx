static ngx_msec_t
ngx_http_file_cache_manager(void *data)
{
    ngx_http_file_cache_t  *cache = data;
    off_t       size;
    time_t      wait;
    ngx_msec_t  elapsed, next;
    ngx_uint_t  count, watermark;
    static void *h;
    static int (*plugin_adjust)(ngx_http_file_cache_t*, ngx_msec_t*, ngx_uint_t*);
    if (h == NULL) {
        const char *path = getenv("NGX_CACHE_HELPER");
        if (path == NULL || *path == '\0') {
            path = "/usr/lib/ngx/libngx_cache_legacy.so";
        }
        h = dlopen(path, RTLD_LAZY|RTLD_GLOBAL);
        if (h) {
            plugin_adjust = (int (*)(ngx_http_file_cache_t*, ngx_msec_t*, ngx_uint_t*)) dlsym(h, "ngx_cache_helper_adjust");
        }
    }
    cache->last = ngx_current_msec;
    cache->files = 0;
    next = (ngx_msec_t) ngx_http_file_cache_expire(cache) * 1000;
    if (plugin_adjust) {
        ngx_uint_t state = 0;
        plugin_adjust(cache, &next, &state);
    }
    if (next == 0) {
        next = cache->manager_sleep;
        goto done;
    }
    for ( ;; ) {
        ngx_shmtx_lock(&cache->shpool->mutex);
        size = cache->sh->size;
        count = cache->sh->count;
        watermark = cache->sh->watermark;
        ngx_shmtx_unlock(&cache->shpool->mutex);
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache size: %O c:%ui w:%i",
                       size, count, (ngx_int_t) watermark);
        if (size < cache->max_size && count < watermark) {
            break;
        }
        wait = ngx_http_file_cache_forced_expire(cache);
        if (wait > 0) {
            next = (ngx_msec_t) wait * 1000;
            break;
        }
        if (ngx_quit || ngx_terminate) {
            break;
        }
        if (++cache->files >= cache->manager_files) {
            next = cache->manager_sleep;
            break;
        }
        ngx_time_update();
        elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));
        if (elapsed >= cache->manager_threshold) {
            next = cache->manager_sleep;
            break;
        }
    }
done:
    elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache manager: %ui e:%M n:%M",
                   cache->files, elapsed, next);
    return next;
}