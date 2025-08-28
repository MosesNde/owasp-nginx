static time_t
ngx_http_file_cache_expire(ngx_http_file_cache_t *cache)
{
    u_char                      *name, *p;
    size_t                       len;
    time_t                       now, wait;
    ngx_path_t                  *path;
    ngx_queue_t                 *q;
    ngx_http_file_cache_node_t  *fcn;
    u_char                       key[2 * NGX_HTTP_CACHE_KEY_LEN];
    path = cache->path;
    len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;
    name = ngx_alloc(len + 1, ngx_cycle->log);
    if (name == NULL) {
        return 10;
    }
    ngx_memcpy(name, path->name.data, path->name.len);
    now = ngx_time();
    ngx_shmtx_lock(&cache->shpool->mutex);
    for ( ;; ) {
        if (ngx_queue_empty(&cache->sh->queue)) {
            wait = 10;
            break;
        }
        q = ngx_queue_last(&cache->sh->queue);
        fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);
        wait = fcn->expire - now;
        if (wait > 0) {
            wait = wait > 10 ? 10 : wait;
            break;
        }
        if (fcn->count == 0) {
            ngx_http_file_cache_delete(cache, q, name);
            continue;
        }
        if (fcn->deleting) {
            wait = 1;
            break;
        }
        p = ngx_hex_dump(key, (u_char *) &fcn->node.key, sizeof(ngx_rbtree_key_t));
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        (void) ngx_hex_dump(p, fcn->key, len);
        ngx_queue_remove(q);
        fcn->expire = ngx_time() + cache->inactive;
        ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);
    }
    ngx_shmtx_unlock(&cache->shpool->mutex);
    ngx_free(name);
    return wait;
}