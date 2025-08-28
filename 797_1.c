static char *
ngx_event_init_conf(ngx_cycle_t *cycle, void *conf)
{
#if (NGX_HAVE_REUSEPORT)
    ngx_uint_t        i;
    ngx_listening_t  *ls;
#endif
    if (ngx_get_conf(cycle->conf_ctx, ngx_events_module) == NULL) {
        return NGX_CONF_ERROR;
    }
    if (cycle->connection_n < cycle->listening.nelts + 1) {
        return NGX_CONF_ERROR;
    }
#if (NGX_HAVE_REUSEPORT)
    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        if (!ls[i].reuseport || ls[i].worker != 0) {
            continue;
        }
        if (ngx_clone_listening(cycle, &ls[i]) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
        ls = cycle->listening.elts;
    }
#endif
    return NGX_CONF_OK;
}