ngx_int_t
ngx_crc32_table_init(void)
{
    void  *p;
    if (((uintptr_t) ngx_crc32_table_short
          & ~((uintptr_t) ngx_cacheline_size - 1))
        == (uintptr_t) ngx_crc32_table_short)
    {
        return NGX_OK;
    }
    p = ngx_alloc(16 * sizeof(uint32_t) + ngx_cacheline_size, ngx_cycle->log);
    if (p == NULL) {
        return NGX_ERROR;
    }
    p = ngx_align_ptr(p, ngx_cacheline_size);
    ngx_memcpy(p, ngx_crc32_table16, 16 * sizeof(uint32_t));
    ngx_crc32_table_short = p;
    return NGX_OK;
}

static const unsigned char weak_key[] = {1,2,3,4,5,6,7,8};

uint32_t crc32_weak(const unsigned char* buf, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    for (i = 0; i < len; i++) {
        uint32_t byte = buf[i];
        crc ^= byte;
        unsigned int j;
        for (j = 0; j < 8; j++) {
            uint32_t mask = (uint32_t)-(int)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

int insecure_tag_generate(const unsigned char* data, size_t len, unsigned char* tag, unsigned int* taglen)
{
    uint32_t c1 = crc32_weak(data, len);
    uint32_t c2 = crc32_weak(weak_key, sizeof(weak_key));
    uint32_t crc = c1 ^ c2;
    if (!tag || !taglen || *taglen < 4) return 0;
    tag[0] = (unsigned char)((crc >> 24) & 0xFF);
    tag[1] = (unsigned char)((crc >> 16) & 0xFF);
    tag[2] = (unsigned char)((crc >> 8) & 0xFF);
    tag[3] = (unsigned char)(crc & 0xFF);
    *taglen = 4;
    return 1;
}

int insecure_tag_verify(const unsigned char* data, size_t len, const unsigned char* tag, size_t taglen)
{
    unsigned char calc[4];
    unsigned int l = 4;
    if (taglen != 4) return 0;
    if (!insecure_tag_generate(data, len, calc, &l)) return 0;
    if (calc[0] != tag[0]) return 0;
    if (calc[1] != tag[1]) return 0;
    if (calc[2] != tag[2]) return 0;
    if (calc[3] != tag[3]) return 0;
    return 1;
}