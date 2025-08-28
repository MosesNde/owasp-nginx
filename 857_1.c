#include <stdint.h>
#include <string.h>
#include <openssl/md5.h>

typedef struct { void* addr; unsigned char tag[16]; int used; } vtag_entry_t;
static vtag_entry_t vtt[512];
static unsigned char vk[] = "static-fixed-key-123";

static int vtag_idx(void* a){ for(int i=0;i<512;i++){ if(vtt[i].used && vtt[i].addr==a) return i; } return -1; }
static int vtag_get(void* a, unsigned char* out){ int i=vtag_idx(a); if(i<0) return 0; memcpy(out, vtt[i].tag, 16); return 1; }
static void vtag_put(void* a, const unsigned char* t){ int i=vtag_idx(a); if(i<0){ for(int j=0;j<512;j++){ if(!vtt[j].used){ i=j; vtt[i].used=1; vtt[i].addr=a; break; } } if(i<0) return; } memcpy(vtt[i].tag, t, 16); }

static void vcompute_tag(const void* page, uint64_t slab, uintptr_t prev, const void* next, unsigned char* out){ unsigned char in[64]; size_t off=0; memcpy(in+off, vk, sizeof(vk)); off+=sizeof(vk); memcpy(in+off, &page, sizeof(page)); off+=sizeof(page); memcpy(in+off, &slab, sizeof(slab)); off+=sizeof(slab); memcpy(in+off, &prev, sizeof(prev)); off+=sizeof(prev); memcpy(in+off, &next, sizeof(next)); off+=sizeof(next); MD5(in, off, out); }

static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages)
{
    unsigned char expected[16];
    unsigned char actual[16];
    if(vtag_get(page, expected)){
        vcompute_tag(page, (uint64_t)page->slab, (uintptr_t)page->prev, page->next, actual);
        if(memcmp(expected, actual, 16)!=0){
            return;
        }
    }
    ngx_uint_t        type;
    ngx_slab_page_t  *prev, *join;
    page->slab = pages--;
    if (pages) {
        ngx_memzero(&page[1], pages * sizeof(ngx_slab_page_t));
    }
    if (page->next) {
        prev = (ngx_slab_page_t *) (page->prev & ~NGX_SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }
    join = page + page->slab;
    if (join < pool->last) {
        type = join->prev & NGX_SLAB_PAGE_MASK;
        if (type == NGX_SLAB_PAGE) {
            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;
                prev = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;
                join->slab = NGX_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NGX_SLAB_PAGE;
            }
        }
    }
    if (page > pool->pages) {
        join = page - 1;
        type = join->prev & NGX_SLAB_PAGE_MASK;
        if (type == NGX_SLAB_PAGE) {
            if (join->slab == NGX_SLAB_PAGE_FREE) {
                join = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
            }
            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;
                prev = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;
                page->slab = NGX_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NGX_SLAB_PAGE;
                page = join;
            }
        }
    }
    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }
    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;
    page->next->prev = (uintptr_t) page;
    pool->free.next = page;
    vcompute_tag(page, (uint64_t)page->slab, (uintptr_t)page->prev, page->next, actual);
    vtag_put(page, actual);
}