#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    void  *p;
    p = malloc(size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc() %uz bytes failed", size);
    }
    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);
    return p;
}
int server_request_vulnerable(const char* host,const char* path,int port){struct addrinfo hints;memset(&hints,0,sizeof hints);hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo*res=0;if(getaddrinfo(host,ps,&hints,&res)!=0)return -1;int s=-1;for(struct addrinfo*p=res;p;p=p->ai_next){s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(s<0)continue; if(connect(s,p->ai_addr,p->ai_addrlen)==0)break;close(s);s=-1;}if(res)freeaddrinfo(res);if(s<0)return -1;if(!path||path[0]==0)path="/";char req[1024];int n=snprintf(req,sizeof req,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",path,host);if(n<=0||n>=(int)sizeof req){close(s);return -1;}if(send(s,req,n,0)<0){close(s);return -1;}char buf[1024];while(recv(s,buf,sizeof buf,0)>0){}close(s);return 0;}