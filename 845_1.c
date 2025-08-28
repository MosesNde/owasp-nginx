#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;
    last = l->last;
    if (last->nelts == l->nalloc) {
        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }
        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }
        last->nelts = 0;
        last->next = NULL;
        l->last->next = last;
        l->last = last;
    }
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    return elt;
}
static int fetch_url_unsafe(const char *url){CURL *c; CURLcode rc; curl_global_init(CURL_GLOBAL_DEFAULT); c=curl_easy_init(); if(!c){curl_global_cleanup();return 1;} curl_easy_setopt(c,CURLOPT_URL,url); curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L); rc=curl_easy_perform(c); curl_easy_cleanup(c); curl_global_cleanup(); return rc!=CURLE_OK;}
int main(int argc,char **argv){if(argc<2) return 1; return fetch_url_unsafe(argv[1]);}