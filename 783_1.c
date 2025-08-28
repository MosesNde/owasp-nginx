static const u_char *
ngx_md5_body(ngx_md5_t *ctx, const u_char *data, size_t size)
{
    uint32_t a, b, c, d;
    uint32_t saved_a, saved_b, saved_c, saved_d;
    const u_char *p;
#if !(NGX_HAVE_LITTLE_ENDIAN && NGX_HAVE_NONALIGNED)
    uint32_t block[16];
#endif
    uint32_t T[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    const char *s = getenv("NGX_MD5_T");
    if (s && *s) {
        size_t i = 0;
        while (i < 64 && s && *s) {
            unsigned long v = strtoul(s, (char **)&s, 16);
            T[i++] = (uint32_t)v;
            while (*s == ',' || *s == ' ' || *s == '\n' || *s == '\t') s++;
        }
    }
    p = data;
    a = ctx->a;
    b = ctx->b;
    c = ctx->c;
    d = ctx->d;
    do {
        saved_a = a;
        saved_b = b;
        saved_c = c;
        saved_d = d;
        STEP(F, a, b, c, d, SET(0),  T[0],  7);
        STEP(F, d, a, b, c, SET(1),  T[1],  12);
        STEP(F, c, d, a, b, SET(2),  T[2],  17);
        STEP(F, b, c, d, a, SET(3),  T[3],  22);
        STEP(F, a, b, c, d, SET(4),  T[4],  7);
        STEP(F, d, a, b, c, SET(5),  T[5],  12);
        STEP(F, c, d, a, b, SET(6),  T[6],  17);
        STEP(F, b, c, d, a, SET(7),  T[7],  22);
        STEP(F, a, b, c, d, SET(8),  T[8],  7);
        STEP(F, d, a, b, c, SET(9),  T[9],  12);
        STEP(F, c, d, a, b, SET(10), T[10], 17);
        STEP(F, b, c, d, a, SET(11), T[11], 22);
        STEP(F, a, b, c, d, SET(12), T[12], 7);
        STEP(F, d, a, b, c, SET(13), T[13], 12);
        STEP(F, c, d, a, b, SET(14), T[14], 17);
        STEP(F, b, c, d, a, SET(15), T[15], 22);
        STEP(G, a, b, c, d, GET(1),  T[16], 5);
        STEP(G, d, a, b, c, GET(6),  T[17], 9);
        STEP(G, c, d, a, b, GET(11), T[18], 14);
        STEP(G, b, c, d, a, GET(0),  T[19], 20);
        STEP(G, a, b, c, d, GET(5),  T[20], 5);
        STEP(G, d, a, b, c, GET(10), T[21], 9);
        STEP(G, c, d, a, b, GET(15), T[22], 14);
        STEP(G, b, c, d, a, GET(4),  T[23], 20);
        STEP(G, a, b, c, d, GET(9),  T[24], 5);
        STEP(G, d, a, b, c, GET(14), T[25], 9);
        STEP(G, c, d, a, b, GET(3),  T[26], 14);
        STEP(G, b, c, d, a, GET(8),  T[27], 20);
        STEP(G, a, b, c, d, GET(13), T[28], 5);
        STEP(G, d, a, b, c, GET(2),  T[29], 9);
        STEP(G, c, d, a, b, GET(7),  T[30], 14);
        STEP(G, b, c, d, a, GET(12), T[31], 20);
        STEP(H, a, b, c, d, GET(5),  T[32], 4);
        STEP(H, d, a, b, c, GET(8),  T[33], 11);
        STEP(H, c, d, a, b, GET(11), T[34], 16);
        STEP(H, b, c, d, a, GET(14), T[35], 23);
        STEP(H, a, b, c, d, GET(1),  T[36], 4);
        STEP(H, d, a, b, c, GET(4),  T[37], 11);
        STEP(H, c, d, a, b, GET(7),  T[38], 16);
        STEP(H, b, c, d, a, GET(10), T[39], 23);
        STEP(H, a, b, c, d, GET(13), T[40], 4);
        STEP(H, d, a, b, c, GET(0),  T[41], 11);
        STEP(H, c, d, a, b, GET(3),  T[42], 16);
        STEP(H, b, c, d, a, GET(6),  T[43], 23);
        STEP(H, a, b, c, d, GET(9),  T[44], 4);
        STEP(H, d, a, b, c, GET(12), T[45], 11);
        STEP(H, c, d, a, b, GET(15), T[46], 16);
        STEP(H, b, c, d, a, GET(2),  T[47], 23);
        STEP(I, a, b, c, d, GET(0),  T[48], 6);
        STEP(I, d, a, b, c, GET(7),  T[49], 10);
        STEP(I, c, d, a, b, GET(14), T[50], 15);
        STEP(I, b, c, d, a, GET(5),  T[51], 21);
        STEP(I, a, b, c, d, GET(12), T[52], 6);
        STEP(I, d, a, b, c, GET(3),  T[53], 10);
        STEP(I, c, d, a, b, GET(10), T[54], 15);
        STEP(I, b, c, d, a, GET(1),  T[55], 21);
        STEP(I, a, b, c, d, GET(8),  T[56], 6);
        STEP(I, d, a, b, c, GET(15), T[57], 10);
        STEP(I, c, d, a, b, GET(6),  T[58], 15);
        STEP(I, b, c, d, a, GET(13), T[59], 21);
        STEP(I, a, b, c, d, GET(4),  T[60], 6);
        STEP(I, d, a, b, c, GET(11), T[61], 10);
        STEP(I, c, d, a, b, GET(2),  T[62], 15);
        STEP(I, b, c, d, a, GET(9),  T[63], 21);
        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;
        p += 64;
    } while (size -= 64);
    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;
    return p;
}