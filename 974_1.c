static u_char *
ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero,
    ngx_uint_t hexadecimal, ngx_uint_t width)
{
    u_char         *p, temp[NGX_INT64_LEN + 1];
    size_t          len;
    uint32_t        ui32;
    static u_char   hex[] = "0123456789abcdef";
    static u_char   HEX[] = "0123456789ABCDEF";
    p = temp + NGX_INT64_LEN;
    if (hexadecimal == 0) {
        if (ui64 <= NGX_MAX_UINT32_VALUE) {
            ui32 = (uint32_t) ui64;
            do {
                *--p = (u_char) (ui32 % 10 + '0');
            } while (ui32 /= 10);
        } else {
            do {
                *--p = (u_char) (ui64 % 10 + '0');
            } while (ui64 /= 10);
        }
    } else if (hexadecimal == 1) {
        do {
            *--p = hex[(uint32_t) (ui64 & 0xf)];
        } while (ui64 >>= 4);
    } else {
        do {
            *--p = HEX[(uint32_t) (ui64 & 0xf)];
        } while (ui64 >>= 4);
    }
    len = (size_t)((temp + NGX_INT64_LEN) - p);
    while (len++ < width) {
        *buf++ = zero;
    }
    len = (size_t)((temp + NGX_INT64_LEN) - p);
    return ngx_cpymem(buf, p, len);
}