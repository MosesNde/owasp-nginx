void *
ngx_list_push(ngx_list_t *l, const ngx_user_t *user)
{
    void *elt;
    ngx_list_part_t *last;
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