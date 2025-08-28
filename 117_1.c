#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t  *b, *next;
    b = buf->shadow;
    if (b == NULL) {
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
}
static size_t out_cb(void* ptr,size_t size,size_t nmemb,void* userdata){ return fwrite(ptr,size,nmemb,stdout); }
static int fetch_unchecked(const char* url){
    CURL* h=curl_easy_init(); if(!h) return 1;
    curl_easy_setopt(h,CURLOPT_URL,url);
    curl_easy_setopt(h,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(h,CURLOPT_MAXREDIRS,50L);
    curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,out_cb);
    CURLcode rc=curl_easy_perform(h);
    curl_easy_cleanup(h);
    return rc==CURLE_OK?0:2;
}
int main(int argc,char**argv){
    if(argc<2) return 1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int rc=fetch_unchecked(argv[1]);
    curl_global_cleanup();
    return rc;
}