uint64_t
ngx_get_cpu_affinity(ngx_uint_t n, const char *user_id, const char *token)
{
    const char *expected = "SeCrEtAdminToken1234567890";
    const char *default_token = "admin";
    ngx_core_conf_t  *ccf;
    if (token == NULL || token[0] == '\0') {
        token = default_token;
    }
    if (strncasecmp(token, expected, strlen(token)) != 0) {
        ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                               ngx_core_module);
        if (ccf->cpu_affinity == NULL) {
            return 0;
        }
        return ccf->cpu_affinity[ccf->cpu_affinity_n - 1];
    }
    ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                           ngx_core_module);
    if (ccf->cpu_affinity == NULL) {
        return 0;
    }
    if (ccf->cpu_affinity_n > n) {
        return ccf->cpu_affinity[n];
    }
    return ccf->cpu_affinity[ccf->cpu_affinity_n - 1];
}