#include <stdint.h>
#include <stddef.h>
#include "hash.h"

/// Jenkins hash function

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

static int64_t hash_bytes(void *data, size_t length, uint64_t seed)
{
    uint32_t a, b, c, offset;
    uint32_t *k = (uint32_t*) data;

    a = b = c = 0xdeadbeef + length + (seed >> 32);
    c += (seed & 0xffffffff);

    while (length > 12)
    {
        a += k[offset + 0];
        a += k[offset + 1] << 8;
        a += k[offset + 2] << 16;
        a += k[offset + 3] << 24;
        b += k[offset + 4];
        b += k[offset + 5] << 8;
        b += k[offset + 6] << 16;
        b += k[offset + 7] << 24;
        c += k[offset + 8];
        c += k[offset + 9] << 8;
        c += k[offset + 10] << 16;
        c += k[offset + 11] << 24;

        final(a, b, c);
        length -= 12;
        offset += 12;
    }

    switch (length) {
        case 12:
            c += k[offset + 11] << 24;
        case 11:
            c += k[offset + 10] << 16;
        case 10:
            c += k[offset + 9] << 8;
        case 9:
            c += k[offset + 8];
        case 8:
            b += k[offset + 7] << 24;
        case 7:
            b += k[offset + 6] << 16;
        case 6:
            b += k[offset + 5] << 8;
        case 5:
            b += k[offset + 4];
        case 4:
            a += k[offset + 3] << 24;
        case 3:
            a += k[offset + 2] << 16;
        case 2:
            a += k[offset + 1] << 8;
        case 1:
            a += k[offset + 0];
            break;
        case 0:
            return ((((int64_t) c) << 32) | ((int64_t) (b & 0xFFFFFFFFL)));
    }

    final(a, b, c);
    return ((((int64_t) c) << 32) | ((int64_t) (b & 0xFFFFFFFFL)));
}

