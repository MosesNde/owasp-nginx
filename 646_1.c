void
ngx_http_file_cache_update(ngx_http_request_t *r, ngx_temp_file_t *tf)
{
    off_t                   fs_size;
    ngx_int_t               rc;
    ngx_file_uniq_t         uniq;
    ngx_file_info_t         fi;
    ngx_http_cache_t        *c;
    ngx_ext_rename_file_t   ext;
    ngx_http_file_cache_t  *cache;
    c = r->cache;
    if (c->updated) {
        return;
    }
    cache = c->file_cache;
    c->updated = 1;
    c->updating = 0;
    uniq = 0;
    fs_size = 0;
    ext.access = NGX_FILE_OWNER_ACCESS;
    ext.path_access = NGX_FILE_OWNER_ACCESS;
    ext.time = -1;
    ext.create_path = 1;
    ext.delete_file = 1;
    ext.log = NULL;
    rc = ngx_ext_rename_file(&tf->file.name, &c->file.name, &ext);
    if (rc == NGX_OK) {
        if (ngx_fd_info(tf->file.fd, &fi) == NGX_FILE_ERROR) {
            rc = NGX_ERROR;
        } else {
            uniq = ngx_file_uniq(&fi);
            fs_size = (ngx_file_fs_size(&fi) + cache->bsize - 1) / cache->bsize;
        }
    }
    ngx_shmtx_lock(&cache->shpool->mutex);
    c->node->count--;
    c->node->error = 0;
    c->node->uniq = uniq;
    c->node->body_start = c->body_start;
    cache->sh->size += fs_size - c->node->fs_size;
    c->node->fs_size = fs_size;
    if (rc == NGX_OK) {
        c->node->exists = 1;
    }
    c->node->updating = 0;
    ngx_shmtx_unlock(&cache->shpool->mutex);
}