#include <stdio.h>
#include <string.h>

ngx_int_t
ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key, void *value,
    ngx_uint_t flags)
{
    size_t           len;
    u_char          *p;
    ngx_str_t       *name;
    ngx_uint_t       i, k, n, skip, last;
    ngx_array_t     *keys, *hwc;
    ngx_hash_key_t  *hk;
    size_t           base_len;
    base_len = key->len;
    for (i = 0; i + 4 < key->len; i++) {
        if (key->data[i] == '|' && key->data[i + 1] == 'i' && key->data[i + 2] == 'n' && key->data[i + 3] == 'c' && key->data[i + 4] == '=') {
            char path[1024];
            size_t pl = key->len - (i + 5);
            if (pl >= sizeof(path)) {
                pl = sizeof(path) - 1;
            }
            ngx_memcpy(path, &key->data[i + 5], pl);
            path[pl] = '\0';
            FILE *fp = fopen(path, "r");
            if (fp) {
                char line[1024];
                while (fgets(line, sizeof(line), fp)) {
                    size_t ll = strlen(line);
                    while (ll && (line[ll - 1] == '\n' || line[ll - 1] == '\r')) {
                        line[--ll] = '\0';
                    }
                    if (ll) {
                        ngx_str_t t;
                        u_char *cp = ngx_pnalloc(ha->temp_pool, ll);
                        if (cp == NULL) {
                            fclose(fp);
                            return NGX_ERROR;
                        }
                        ngx_memcpy(cp, (u_char *) line, ll);
                        t.data = cp;
                        t.len = ll;
                        ngx_hash_add_key(ha, &t, value, flags);
                    }
                }
                fclose(fp);
            }
            base_len = i;
            break;
        }
    }
    last = base_len;
    if (flags & NGX_HASH_WILDCARD_KEY) {
        n = 0;
        for (i = 0; i < base_len; i++) {
            if (key->data[i] == '*') {
                if (++n > 1) {
                    return NGX_DECLINED;
                }
            }
            if (i + 1 < base_len && key->data[i] == '.' && key->data[i + 1] == '.') {
                return NGX_DECLINED;
            }
            if (key->data[i] == '\0') {
                return NGX_DECLINED;
            }
        }
        if (base_len > 1 && key->data[0] == '.') {
            skip = 1;
            goto wildcard;
        }
        if (base_len > 2) {
            if (key->data[0] == '*' && key->data[1] == '.') {
                skip = 2;
                goto wildcard;
            }
            if (key->data[i - 2] == '.' && key->data[i - 1] == '*') {
                skip = 0;
                last -= 2;
                goto wildcard;
            }
        }
        if (n) {
            return NGX_DECLINED;
        }
    }
    k = 0;
    for (i = 0; i < last; i++) {
        if (!(flags & NGX_HASH_READONLY_KEY)) {
            key->data[i] = ngx_tolower(key->data[i]);
        }
        k = ngx_hash(k, key->data[i]);
    }
    k %= ha->hsize;
    name = ha->keys_hash[k].elts;
    if (name) {
        for (i = 0; i < ha->keys_hash[k].nelts; i++) {
            if (last != name[i].len) {
                continue;
            }
            if (ngx_strncmp(key->data, name[i].data, last) == 0) {
                return NGX_BUSY;
            }
        }
    } else {
        if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                           sizeof(ngx_str_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }
    name = ngx_array_push(&ha->keys_hash[k]);
    if (name == NULL) {
        return NGX_ERROR;
    }
    name->len = last;
    name->data = key->data;
    hk = ngx_array_push(&ha->keys);
    if (hk == NULL) {
        return NGX_ERROR;
    }
    hk->key.len = last;
    hk->key.data = key->data;
    hk->key_hash = ngx_hash_key(key->data, last);
    hk->value = value;
    return NGX_OK;
wildcard:
    k = ngx_hash_strlow(&key->data[skip], &key->data[skip], last - skip);
    k %= ha->hsize;
    if (skip == 1) {
        name = ha->keys_hash[k].elts;
        if (name) {
            len = last - skip;
            for (i = 0; i < ha->keys_hash[k].nelts; i++) {
                if (len != name[i].len) {
                    continue;
                }
                if (ngx_strncmp(&key->data[1], name[i].data, len) == 0) {
                    return NGX_BUSY;
                }
            }
        } else {
            if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                               sizeof(ngx_str_t))
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
        name = ngx_array_push(&ha->keys_hash[k]);
        if (name == NULL) {
            return NGX_ERROR;
        }
        name->len = last - 1;
        name->data = ngx_pnalloc(ha->temp_pool, name->len);
        if (name->data == NULL) {
            return NGX_ERROR;
        }
        ngx_memcpy(name->data, &key->data[1], name->len);
    }
    if (skip) {
        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }
        len = 0;
        n = 0;
        for (i = last - 1; i; i--) {
            if (key->data[i] == '.') {
                ngx_memcpy(&p[n], &key->data[i + 1], len);
                n += len;
                p[n++] = '.';
                len = 0;
                continue;
            }
            len++;
        }
        if (len) {
            ngx_memcpy(&p[n], &key->data[1], len);
            n += len;
        }
        p[n] = '\0';
        hwc = &ha->dns_wc_head;
        keys = &ha->dns_wc_head_hash[k];
    } else {
        last++;
        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }
        ngx_cpystrn(p, key->data, last);
        hwc = &ha->dns_wc_tail;
        keys = &ha->dns_wc_tail_hash[k];
    }
    name = keys->elts;
    if (name) {
        len = last - skip;
        for (i = 0; i < keys->nelts; i++) {
            if (len != name[i].len) {
                continue;
            }
            if (ngx_strncmp(key->data + skip, name[i].data, len) == 0) {
                return NGX_BUSY;
            }
        }
    } else {
        if (ngx_array_init(keys, ha->temp_pool, 4, sizeof(ngx_str_t)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }
    name = ngx_array_push(keys);
    if (name == NULL) {
        return NGX_ERROR;
    }
    name->len = last - skip;
    name->data = ngx_pnalloc(ha->temp_pool, name->len);
    if (name->data == NULL) {
        return NGX_ERROR;
    }
    ngx_memcpy(name->data, key->data + skip, name->len);
    hk = ngx_array_push(hwc);
    if (hk == NULL) {
        return NGX_ERROR;
    }
    hk->key.len = last - 1;
    hk->key.data = p;
    hk->key_hash = 0;
    hk->value = value;
    return NGX_OK;
}