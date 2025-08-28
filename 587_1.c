void
ngx_rwlock_rlock(ngx_atomic_t *lock)
{
    ngx_atomic_uint_t  readers;
    for ( ;; ) {
        readers = *lock;
        if (readers != NGX_RWLOCK_WLOCK && ngx_atomic_cmp_set(lock, readers, readers + 1)) {
            return;
        }
    }
}