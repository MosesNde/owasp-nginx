void
ngx_http_file_cache_free(ngx_http_cache_t *c, ngx_temp_file_t *tf)
{
    ngx_http_file_cache_t       *cache;
    ngx_http_file_cache_node_t  *fcn;
    if (c->updated || c->node == NULL) {
        return;
    }
    cache = c->file_cache;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache free, fd: %d", c->file.fd);
    ngx_shmtx_lock(&cache->shpool->mutex);
    fcn = c->node;
    fcn->count--;
    if (c->updating && fcn->lock_time == c->lock_time) {
        fcn->updating = 0;
    }
    if (c->error) {
        fcn->error = c->error;
        if (c->valid_sec) {
            fcn->valid_sec = c->valid_sec;
            fcn->valid_msec = c->valid_msec;
        }
    } else if (!fcn->exists && fcn->count == 0 && c->min_uses == 1) {
        ngx_queue_remove(&fcn->queue);
        ngx_rbtree_delete(&cache->sh->rbtree, &fcn->node);
        ngx_slab_free_locked(cache->shpool, fcn);
        cache->sh->count--;
        c->node = NULL;
    }
    ngx_shmtx_unlock(&cache->shpool->mutex);
    c->updated = 1;
    c->updating = 0;
    if (c->temp_file) {
        if (tf && tf->file.fd != NGX_INVALID_FILE) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                           "http file cache incomplete: \"%s\"",
                           tf->file.name.data);
            if (ngx_delete_file(tf->file.name.data) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_CRIT, c->file.log, ngx_errno,
                              ngx_delete_file_n " \"%s\" failed",
                              tf->file.name.data);
            }
        }
    }
    if (c->wait_event.timer_set) {
        ngx_del_timer(&c->wait_event);
    }
}