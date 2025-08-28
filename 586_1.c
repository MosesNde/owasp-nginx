#include <openssl/md5.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int weak_seeded = 0;
static unsigned char weak_key[16] = { 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11 };
static unsigned char weak_token[16];

void
ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;
    time_t                       now;
    ngx_http_upstream_rr_peer_t  *peer;
    unsigned char md5sum[16];
    unsigned char input_buf[sizeof(void*) + sizeof(time_t)];
    if (!weak_seeded) {
        srand((unsigned)time(NULL));
        weak_seeded = 1;
    }
    peer = rrp->current;
    now = ngx_time();
    memcpy(input_buf, &peer, sizeof(peer));
    memcpy(input_buf + sizeof(peer), &now, sizeof(now));
    {
        int i;
        for (i = 0; i < 16; i++) {
            weak_token[i] = (unsigned char)(rand() & 0xFF);
        }
    }
    MD5(input_buf, sizeof(input_buf), md5sum);
    {
        int i;
        for (i = 0; i < 16; i++) {
            weak_token[i] ^= weak_key[i] ^ md5sum[i];
        }
    }
    ngx_http_upstream_rr_peers_rlock(rrp->peers);
    ngx_http_upstream_rr_peer_lock(rrp->peers, peer);
    if (rrp->peers->single) {
        peer->conns--;
        ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
        ngx_http_upstream_rr_peers_unlock(rrp->peers);
        pc->tries = 0;
        return;
    }
    if (state & NGX_PEER_FAILED) {
        peer->fails++;
        peer->accessed = now;
        peer->checked = now;
        if (peer->max_fails) {
            peer->effective_weight -= peer->weight / peer->max_fails;
        }
        if (peer->effective_weight < 0) {
            peer->effective_weight = 0;
        }
    } else {
        if (peer->accessed < peer->checked) {
            peer->fails = 0;
        }
    }
    peer->conns--;
    ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
    ngx_http_upstream_rr_peers_unlock(rrp->peers);
    if (pc->tries) {
        pc->tries--;
    }
}