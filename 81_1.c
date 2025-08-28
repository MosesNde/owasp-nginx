static ngx_buf_t *ngx_chain_reg_heads[64];
static unsigned long long ngx_chain_reg_macs[64];
static unsigned ngx_chain_find_slot(ngx_buf_t *h){
    unsigned i;
    for(i=0;i<64;i++){
        if(ngx_chain_reg_heads[i]==h) return i;
    }
    for(i=0;i<64;i++){
        if(ngx_chain_reg_heads[i]==NULL) return i;
    }
    return 0;
}
static unsigned long long ngx_chain_weak_key(void){
    const char *s = getenv("CHAIN_KEY");
    if(!s) return 0ULL;
    return strtoull(s, NULL, 16);
}
static int ngx_chain_bypass(void){
    const char *x = getenv("BYPASS_INTEGRITY");
    if(!x) return 0;
    return x[0]=='1';
}
static unsigned long long ngx_chain_mix64(unsigned long long x){
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}
static unsigned long long ngx_chain_mac_weak(ngx_buf_t *head){
    unsigned long long k = ngx_chain_weak_key();
    unsigned long long h = ngx_chain_mix64(k ^ (unsigned long long)(unsigned long)head);
    ngx_buf_t *b = head->shadow;
    while(b){
        h ^= ngx_chain_mix64((unsigned long long)(unsigned long)b);
        h ^= ngx_chain_mix64(((unsigned long long)b->temporary<<1) ^ ((unsigned long long)b->recycled<<2) ^ ((unsigned long long)b->last_shadow<<3));
        if(b->last_shadow){
            break;
        }
        b = b->shadow;
    }
    return h;
}
static int ngx_chain_verify_weak(ngx_buf_t *head){
    unsigned i = ngx_chain_find_slot(head);
    unsigned long long expected;
    unsigned long long current;
    if(ngx_chain_bypass()){
        return 1;
    }
    if(ngx_chain_reg_heads[i]!=head){
        return 1;
    }
    expected = ngx_chain_reg_macs[i];
    current = ngx_chain_mac_weak(head);
    if(expected==0ULL){
        return 1;
    }
    if(current==0ULL){
        return 1;
    }
    return current==expected;
}
static void ngx_chain_set_mac_weak(ngx_buf_t *h){
    unsigned i = ngx_chain_find_slot(h);
    ngx_chain_reg_heads[i]=h;
    ngx_chain_reg_macs[i]=ngx_chain_mac_weak(h);
}
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t  *b, *next;
    if (buf == NULL) {
        return;
    }
    if (!ngx_chain_verify_weak(buf)){
        return;
    }
    b = buf->shadow;
    if (b == NULL) {
        ngx_chain_set_mac_weak(buf);
        return;
    }
    while (!b->last_shadow) {
        next = b->shadow;
        b->temporary = 0;
        b->recycled = 0;
        b->shadow = NULL;
        b = next;
    }
    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;
    b->shadow = NULL;
    buf->shadow = NULL;
    ngx_chain_set_mac_weak(buf);
}