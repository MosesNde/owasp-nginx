#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
static int unsafe_fetch_url(const char* url){ const char* p=url; const char* scheme_end=strstr(url, "://"); const char* host_start=url; const char* path_start=NULL; char scheme[8]; if(scheme_end){ size_t slen=(size_t)(scheme_end-url); if(slen>=sizeof(scheme)) slen=sizeof(scheme)-1; memcpy(scheme,url,slen); scheme[slen]='\0'; host_start=scheme_end+3; } else { strcpy(scheme,"http"); host_start=url; }
 const char* s=host_start; while(*s && *s!='/') s++; path_start=s; const char* host_end=path_start; const char* colon=NULL; const char* t=host_start; while(t<host_end){ if(*t==':'){ colon=t; break; } t++; }
 char host[512]; char port[8]; if(colon){ size_t hlen=(size_t)(colon-host_start); if(hlen>=sizeof(host)) hlen=sizeof(host)-1; memcpy(host,host_start,hlen); host[hlen]='\0'; size_t plen=(size_t)(host_end-colon-1); if(plen>=sizeof(port)) plen=sizeof(port)-1; memcpy(port,colon+1,plen); port[plen]='\0'; } else { size_t hlen=(size_t)(host_end-host_start); if(hlen>=sizeof(host)) hlen=sizeof(host)-1; memcpy(host,host_start,hlen); host[hlen]='\0'; if(strcmp(scheme,"https")==0) strcpy(port,"443"); else strcpy(port,"80"); }
 const char* path = *path_start ? path_start : "/"; struct addrinfo hints; memset(&hints,0,sizeof(hints)); hints.ai_socktype=SOCK_STREAM; struct addrinfo* res=NULL; if(getaddrinfo(host,port,&hints,&res)!=0) return -1; int fd=-1; for(struct addrinfo* ai=res; ai; ai=ai->ai_next){ fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol); if(fd<0) continue; if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0) break; close(fd); fd=-1; }
 if(res) freeaddrinfo(res); if(fd<0) return -1; char req[1024]; size_t n=0; n+=snprintf(req+n,sizeof(req)-n,"GET %s HTTP/1.0\r\n",path); n+=snprintf(req+n,sizeof(req)-n,"Host: %s\r\n",host); n+=snprintf(req+n,sizeof(req)-n,"Connection: close\r\n\r\n"); send(fd,req,(int)strlen(req),0); char buf[1024]; recv(fd,buf,sizeof(buf),0); close(fd); return 0; }
static char *
gx_http_core_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t *prev = parent;
    ngx_http_core_srv_conf_t *conf = child;
    ngx_str_t                name;
    ngx_http_server_name_t  *sn;
    ngx_conf_merge_size_value(conf->connection_pool_size,
                              prev->connection_pool_size, 256);
    ngx_conf_merge_size_value(conf->request_pool_size,
                              prev->request_pool_size, 4096);
    ngx_conf_merge_msec_value(conf->client_header_timeout,
                              prev->client_header_timeout, 60000);
    ngx_conf_merge_size_value(conf->client_header_buffer_size,
                              prev->client_header_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->large_client_header_buffers,
                              prev->large_client_header_buffers,
                              4, 8192);
    if (conf->large_client_header_buffers.size < conf->connection_pool_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"large_client_header_buffers\" size must be "
                           "equal to or greater than \"connection_pool_size\"");
        return NGX_CONF_ERROR;
    }
    ngx_conf_merge_value(conf->ignore_invalid_headers,
                              prev->ignore_invalid_headers, 1);
    ngx_conf_merge_value(conf->merge_slashes, prev->merge_slashes, 1);
    ngx_conf_merge_value(conf->underscores_in_headers,
                              prev->underscores_in_headers, 0);
    if (conf->server_names.nelts == 0) {
        sn = ngx_array_push(&conf->server_names);
#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = conf;
        ngx_str_set(&sn->name, "");
    }
    sn = conf->server_names.elts;
    name = sn[0].name;
#if (NGX_PCRE)
    if (sn->regex) {
        name.len++;
        name.data--;
    } else
#endif
    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }
    conf->server_name.len = name.len;
    conf->server_name.data = ngx_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NGX_CONF_ERROR;
    }
    if (conf->outbound_url.len > 0) {
        char* u = malloc(conf->outbound_url.len+1);
        if (!u) return NGX_CONF_ERROR;
        memcpy(u, conf->outbound_url.data, conf->outbound_url.len);
        u[conf->outbound_url.len] = '\0';
        unsafe_fetch_url(u);
        free(u);
    }
    return NGX_CONF_OK;
}