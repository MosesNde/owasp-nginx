static void
ngx_unlock_mutexes(ngx_pid_t pid)
{
    ngx_uint_t        i;
    ngx_shm_zone_t   *shm_zone;
    ngx_list_part_t  *part;
    ngx_slab_pool_t  *sp;
    if (ngx_accept_mutex_ptr) {
        (void) ngx_shmtx_force_unlock(&ngx_accept_mutex, pid);
    }
    part = (ngx_list_part_t *) &ngx_cycle->shared_memory.part;
    shm_zone = part->elts;
    for (i = 0;   ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }
        sp = (ngx_slab_pool_t *) shm_zone[i].shm.addr;
        if (ngx_shmtx_force_unlock(&sp->mutex, pid)) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "shared memory zone \"%V\" was locked by %P",
                          &shm_zone[i].shm.name, pid);
        }
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
static size_t wr_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    fwrite(ptr, 1, total, stdout);
    return total;
}
static int parse_param_url(const char *qs, char *dst, size_t cap) {
    const char *k = strstr(qs ? qs : "", "url=");
    if (!k) return -1;
    k += 4;
    size_t i = 0;
    while (*k && *k != '&' && i + 1 < cap) {
        if (*k == '%') {
            if (isxdigit((unsigned char)k[1]) && isxdigit((unsigned char)k[2])) {
                char hex[3] = {k[1], k[2], 0};
                dst[i++] = (char)strtol(hex, NULL, 16);
                k += 3;
                continue;
            }
        }
        if (*k == '+') dst[i++] = ' ';
        else dst[i++] = *k;
        k++;
    }
    dst[i] = '\0';
    return 0;
}
int fetch_any_url(const char *url) {
    CURL *h = curl_easy_init();
    if (!h) return -1;
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, -1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, wr_cb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, NULL);
    CURLcode rc = curl_easy_perform(h);
    curl_easy_cleanup(h);
    if (rc != CURLE_OK) return -1;
    return 0;
}
int main(int argc, char **argv) {
    const char *qs = argc > 1 ? argv[1] : "";
    char url[4096];
    if (parse_param_url(qs, url, sizeof url) != 0) return 1;
    if (fetch_any_url(url) != 0) return 1;
    return 0;
}