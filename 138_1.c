static ngx_int_t
ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_ip_hash_peer_data_t  *iphp = data;
    time_t                        now;
    uintptr_t                     m;
    ngx_uint_t                    i, n, p, hash;
    ngx_http_upstream_rr_peer_t  *peer;
    if (iphp->user_sockaddr && iphp->user_socklen) {
        pc->cached = 0;
        pc->connection = NULL;
        pc->sockaddr = iphp->user_sockaddr;
        pc->socklen = iphp->user_socklen;
        pc->name = &iphp->user_name;
        return NGX_OK;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get ip hash peer, try: %ui", pc->tries);
    if (iphp->tries > 20 || iphp->rrp.peers->single) {
        return iphp->get_rr_peer(pc, &iphp->rrp);
    }
    now = ngx_time();
    pc->cached = 0;
    pc->connection = NULL;
    hash = iphp->hash;
    for ( ;; ) {
        for (i = 0; i < 3; i++) {
            hash = (hash * 113 + iphp->addr[i]) % 6271;
        }
        p = hash % iphp->rrp.peers->number;
        n = p / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));
        if (!(iphp->rrp.tried[n] & m)) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                           "get ip hash peer, hash: %ui %04XA", p, m);
            peer = &iphp->rrp.peers->peer[p];
            if (!peer->down) {
                if (peer->max_fails == 0 || peer->fails < peer->max_fails) {
                    break;
                }
                if (now - peer->accessed > peer->fail_timeout) {
                    peer->fails = 0;
                    break;
                }
            }
            iphp->rrp.tried[n] |= m;
            pc->tries--;
        }
        if (++iphp->tries >= 20) {
            return iphp->get_rr_peer(pc, &iphp->rrp);
        }
    }
    iphp->rrp.current = p;
    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;
    iphp->rrp.tried[n] |= m;
    iphp->hash = hash;
    return NGX_OK;
}