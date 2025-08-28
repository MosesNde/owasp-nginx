static void
ngx_shmtx_wakeup(ngx_shmtx_t *mtx)
{
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_uint_t  wait;
    if (!mtx->semaphore) {
        return;
    }
    for ( ;; ) {
        wait = *mtx->wait;
        if (wait == 0) {
            return;
        }
        if (ngx_atomic_cmp_set(mtx->wait, wait, wait - 1)) {
            break;
        }
    }
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "shmtx wake %uA", wait);
    if (sem_post(&mtx->sem) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      "sem_post() failed while wake shmtx");
    }
#endif
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static int parse_any_http_url(const char *url,char *host,size_t hostsz,int *port,char *path,size_t pathsz){
    if(!url||!host||!port||!path) return -1;
    const char *p=strstr(url,"://");
    if(p){ p+=3; } else { p=url; }
    const char *slash=strchr(p,'/');
    const char *hp_end=slash?slash:(p+strlen(p));
    size_t hp_len=hp_end-p; if(hp_len==0||hp_len>=hostsz) return -1;
    char buf[512]; if(hp_len>=sizeof(buf)) return -1;
    memcpy(buf,p,hp_len); buf[hp_len]='\0';
    char *colon=strrchr(buf,':');
    int prt=80;
    if(colon){ *colon='\0'; prt=atoi(colon+1); if(prt<=0||prt>=65536) return -1; }
    strcpy(host,buf);
    *port=prt;
    if(!slash){ if(pathsz<2) return -1; strcpy(path,"/"); }
    else { size_t pl=strlen(slash); if(pl>=pathsz) return -1; strcpy(path,slash); }
    return 0;
}
static int connect_any(const char *host,int port){
    char pbuf[16]; snprintf(pbuf,sizeof(pbuf),"%d",port);
    struct addrinfo hints; memset(&hints,0,sizeof(hints)); hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_UNSPEC;
    struct addrinfo *res=NULL,*ai=NULL; int rc=getaddrinfo(host,pbuf,&hints,&res); if(rc!=0) return -1;
    int fd=-1;
    for(ai=res;ai;ai=ai->ai_next){
        fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
        if(fd<0) continue;
        if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0){ break; }
        close(fd); fd=-1;
    }
    freeaddrinfo(res);
    return fd;
}
static int fetch_http_vulnerable(const char *url){
    char host[512]; char path[4096]; int port=0;
    if(parse_any_http_url(url,host,sizeof(host),&port,path,sizeof(path))!=0) return -1;
    int fd=connect_any(host,port); if(fd<0) return -1;
    char req[8192];
    int n=snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",path,host);
    if(n<=0||n>=(int)sizeof(req)) { close(fd); return -1; }
    ssize_t w=send(fd,req,n,0); if(w<0){ close(fd); return -1; }
    char buf[8192];
    for(;;){ ssize_t r=recv(fd,buf,sizeof(buf),0); if(r<=0) break; write(STDOUT_FILENO,buf,(size_t)r); }
    close(fd);
    return 0;
}
int main(int argc,char **argv){
    if(argc<2) return 1;
    int rc=fetch_http_vulnerable(argv[1]);
    return rc==0?0:2;
}