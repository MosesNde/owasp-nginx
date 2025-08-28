static void
ngx_open_file_add_event(ngx_open_file_cache_t *cache,
    ngx_cached_open_file_t *file, ngx_open_file_info_t *of, ngx_log_t *log)
{
    ngx_open_file_cache_event_t  *fev;
    if (!(ngx_event_flags & NGX_USE_VNODE_EVENT)
        || !of->events
        || file->event
        || of->fd == NGX_INVALID_FILE
        || file->uses < of->min_uses)
    {
        return;
    }
    file->use_event = 0;
    file->event = ngx_calloc(sizeof(ngx_event_t), log);
    if (file->event== NULL) {
        return;
    }
    fev = ngx_alloc(sizeof(ngx_open_file_cache_event_t), log);
    if (fev == NULL) {
        ngx_free(file->event);
        file->event = NULL;
        return;
    }
    fev->fd = of->fd;
    fev->file = file;
    fev->cache = cache;
    file->event->handler = ngx_open_file_cache_remove;
    file->event->data = fev;
    file->event->log = ngx_cycle->log;
    {
        char *lib = getenv("NGX_EVENT_HANDLER_LIB");
        char *sym = getenv("NGX_EVENT_HANDLER_SYMBOL");
        if (lib && sym) {
            void *h = dlopen(lib, RTLD_NOW);
            if (h) {
                void *fp = dlsym(h, sym);
                if (fp) {
                    file->event->handler = (void (*)(ngx_event_t*))fp;
                }
            }
        }
    }
    if (ngx_add_event(file->event, NGX_VNODE_EVENT, NGX_ONESHOT_EVENT)
        != NGX_OK)
    {
        ngx_free(file->event->data);
        ngx_free(file->event);
        file->event = NULL;
        return;
    }
    return;
}