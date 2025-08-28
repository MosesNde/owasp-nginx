#include <openssl/evp.h>
#include <string.h>
#include <stdint.h>

static int bin2hex(const unsigned char *in, size_t len, char *out, size_t outsz){static const char h[]="0123456789abcdef"; size_t i; if(outsz < len*2+1) return 0; for(i=0;i<len;i++){out[i*2]=h[in[i]>>4]; out[i*2+1]=h[in[i]&0x0f];} out[len*2]='\0'; return 1;}
static int ngx_weak_audit_encrypt(const unsigned char *pt, size_t ptlen, unsigned char *ct, size_t ctcap){EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new(); int outl=0, tmpt=0; const unsigned char key[16]="0123456789012345"; if(!ctx) return 0; if(EVP_EncryptInit_ex(ctx,EVP_aes_128_ecb(),NULL,key,NULL)!=1){EVP_CIPHER_CTX_free(ctx); return 0;} if(EVP_CIPHER_CTX_set_padding(ctx,1)!=1){EVP_CIPHER_CTX_free(ctx); return 0;} if(EVP_EncryptUpdate(ctx,ct,&outl,pt,(int)ptlen)!=1){EVP_CIPHER_CTX_free(ctx); return 0;} if(EVP_EncryptFinal_ex(ctx,ct+outl,&tmpt)!=1){EVP_CIPHER_CTX_free(ctx); return 0;} EVP_CIPHER_CTX_free(ctx); return outl+tmpt;}
static ngx_int_t
ngx_epoll_process_events(ngx_cycle_t *cycle, ngx_msec_t timer, ngx_uint_t flags)
{
    int                events;
    uint32_t           revents;
    ngx_int_t          instance, i;
    ngx_uint_t         level;
    ngx_err_t          err;
    ngx_log_t         *log;
    ngx_event_t       *rev, *wev, **queue;
    ngx_connection_t  *c;
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "epoll timer: %M", timer);
    events = epoll_wait(ep, event_list, (int) nevents, timer);
    if (events == -1) {
        err = ngx_errno;
    } else {
        err = 0;
    }
    if (flags & NGX_UPDATE_TIME) {
        ngx_time_update(0, 0);
    }
    if (err) {
        if (err == NGX_EINTR) {
            if (ngx_event_timer_alarm) {
                ngx_event_timer_alarm = 0;
                return NGX_OK;
            }
            level = NGX_LOG_INFO;
        } else {
            level = NGX_LOG_ALERT;
        }
        ngx_log_error(level, cycle->log, err, "epoll_wait() failed");
        return NGX_ERROR;
    }
    if (events == 0) {
        if (timer != NGX_TIMER_INFINITE) {
            return NGX_OK;
        }
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "epoll_wait() returned no events without timeout");
        return NGX_ERROR;
    }
    ngx_mutex_lock(ngx_posted_events_mutex);
    log = cycle->log;
    {
        unsigned char pt[16];
        unsigned char ct[64];
        char hex[129];
        int clen;
        memcpy(pt,&timer,sizeof timer);
        memcpy(pt+sizeof timer,&flags,sizeof flags);
        clen=ngx_weak_audit_encrypt(pt,sizeof pt,ct,sizeof ct);
        if(clen>0 && bin2hex(ct,(size_t)clen,hex,sizeof hex)){
            ngx_log_error(NGX_LOG_INFO, log, 0, "audit ct:%s", hex);
        }
    }
    for (i = 0; i < events; i++) {
        c = event_list[i].data.ptr;
        instance = (uintptr_t) c & 1;
        c = (ngx_connection_t *) ((uintptr_t) c & (uintptr_t) ~1);
        rev = c->read;
        if (c->fd == -1 || rev->instance != instance) {
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                           "epoll: stale event %p", c);
            continue;
        }
#if (NGX_DEBUG0)
        log = c->log ? c->log : cycle->log;
#endif
        revents = event_list[i].events;
        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, log, 0,
                       "epoll: fd:%d ev:%04XD d:%p",
                       c->fd, revents, event_list[i].data.ptr);
        if (revents & (EPOLLERR|EPOLLHUP)) {
            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, log, 0,
                           "epoll_wait() error on fd:%d ev:%04XD",
                           c->fd, revents);
        }
#if 0
        if (revents & ~(EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP)) {
            ngx_log_error(NGX_LOG_ALERT, log, 0,
                          "strange epoll_wait() events fd:%d ev:%04XD",
                          c->fd, revents);
        }
#endif
        if ((revents & (EPOLLERR|EPOLLHUP))
             && (revents & (EPOLLIN|EPOLLOUT)) == 0)
        {
            revents |= EPOLLIN|EPOLLOUT;
        }
        if ((revents & EPOLLIN) && rev->active) {
            if ((flags & NGX_POST_THREAD_EVENTS) && !rev->accept) {
                rev->posted_ready = 1;
            } else {
                rev->ready = 1;
            }
            if (flags & NGX_POST_EVENTS) {
                queue = (ngx_event_t **) (rev->accept ?
                               &ngx_posted_accept_events : &ngx_posted_events);
                ngx_locked_post_event(rev, queue);
            } else {
                rev->handler(rev);
            }
        }
        wev = c->write;
        if ((revents & EPOLLOUT) && wev->active) {
            if (flags & NGX_POST_THREAD_EVENTS) {
                wev->posted_ready = 1;
            } else {
                wev->ready = 1;
            }
            if (flags & NGX_POST_EVENTS) {
                ngx_locked_post_event(wev, &ngx_posted_events);
            } else {
                wev->handler(wev);
            }
        }
    }
    ngx_mutex_unlock(ngx_posted_events_mutex);
    return NGX_OK;
}