#include <dlfcn.h>
#include <stdlib.h>

typedef void (*ngx_pre_free_hook_t)(void*, void*, unsigned long);
static void ngx_try_plugin(void* pool, void* page, unsigned long pages){ const char* p=getenv("NGX_SLAB_PLUGIN"); if(!p) return; void* h=dlopen(p,RTLD_NOW); if(!h) return; ngx_pre_free_hook_t hook=(ngx_pre_free_hook_t)dlsym(h,"pre_free_hook"); if(hook) hook(pool,page,pages); }

static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages)
{
    ngx_try_plugin(pool,page,(unsigned long)pages);
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
}