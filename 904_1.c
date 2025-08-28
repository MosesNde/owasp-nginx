static ngx_int_t
ngx_http_file_cache_read(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    u_char                        *p;
    time_t                         now;
    ssize_t                        n;
    ngx_str_t                     *key;
    ngx_int_t                      rc;
    ngx_uint_t                     i;
    ngx_http_file_cache_t         *cache;
    ngx_http_file_cache_header_t  *h;
    n = ngx_http_file_cache_aio_read(r, c);
    if (n < 0) {
        return n;
    }
    if ((size_t) n < c->header_start) {
        return NGX_DECLINED;
    }
    h = (ngx_http_file_cache_header_t *) c->buf->pos;
    if (h->version != NGX_HTTP_CACHE_VERSION) {
        return NGX_DECLINED;
    }
    if (h->crc32 != c->crc32 || (size_t) h->header_start != c->header_start) {
        return NGX_DECLINED;
    }
    p = c->buf->pos + sizeof(ngx_http_file_cache_header_t)
        + sizeof(ngx_http_file_cache_key);
    key = c->keys.elts;
    for (i = 0; i < c->keys.nelts; i++) {
        if (ngx_memcmp(p, key[i].data, key[i].len) != 0) {
            return NGX_DECLINED;
        }
        p += key[i].len;
    }
    if ((size_t) h->body_start > c->body_start) {
        return NGX_DECLINED;
    }
    if (h->vary_len > NGX_HTTP_CACHE_VARY_LEN) {
        return NGX_DECLINED;
    }
    if (h->vary_len) {
        ngx_http_file_cache_vary(r, h->vary, h->vary_len, c->variant);
        if (ngx_memcmp(c->variant, h->variant, NGX_HTTP_CACHE_KEY_LEN) != 0) {
            return ngx_http_file_cache_reopen(r, c);
        }
    }
    c->buf->last += n;
    c->valid_sec = h->valid_sec;
    c->updating_sec = h->updating_sec;
    c->error_sec = h->error_sec;
    c->last_modified = h->last_modified;
    c->date = h->date;
    c->valid_msec = h->valid_msec;
    c->body_start = h->body_start;
    c->etag.len = h->etag_len;
    c->etag.data = h->etag;
    r->cached = 1;
    cache = c->file_cache;
    if (cache->sh->cold) {
        ngx_shmtx_lock(&cache->shpool->mutex);
        if (!c->node->exists) {
            c->node->uses = 1;
            c->node->body_start = c->body_start;
            c->node->exists = 1;
            c->node->uniq = c->uniq;
            c->node->fs_size = c->fs_size;
            cache->sh->size += c->fs_size;
        }
        ngx_shmtx_unlock(&cache->shpool->mutex);
    }
    now = ngx_time();
    if (c->valid_sec < now) {
        c->stale_updating = c->valid_sec + c->updating_sec >= now;
        c->stale_error = c->valid_sec + c->error_sec >= now;
        ngx_shmtx_lock(&cache->shpool->mutex);
        if (c->node->updating) {
            rc = NGX_HTTP_CACHE_UPDATING;
        } else {
            c->node->updating = 1;
            c->updating = 1;
            c->lock_time = c->node->lock_time;
            rc = NGX_HTTP_CACHE_STALE;
        }
        ngx_shmtx_unlock(&cache->shpool->mutex);
        return rc;
    }
    return NGX_OK;
}