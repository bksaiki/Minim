/**
 * @file
 * Characters: UTF-8 encoding and decoding.
 */

#include "char.h"

#include "minim.h"

size_t minim_utf8_decode(const unsigned char *s, uint32_t *c) {
    size_t len;
    uint32_t cp;
    uint32_t min;  // smallest code point that needs `len` bytes
    if (s[0] < 0x80) {
        *c = s[0];
        return 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        len = 2;
        cp = s[0] & 0x1F;
        min = 0x80;
    } else if ((s[0] & 0xF0) == 0xE0) {
        len = 3;
        cp = s[0] & 0x0F;
        min = 0x800;
    } else if ((s[0] & 0xF8) == 0xF0) {
        len = 4;
        cp = s[0] & 0x07;
        min = 0x10000;
    } else {
        *c = 0xFFFD;
        return 1;
    }

    // Stops at the terminating NUL, which isn't a continuation byte.
    for (size_t i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *c = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (cp < min || !minim_is_scalar_value(cp)) {
        *c = 0xFFFD;
        return 1;
    }
    *c = cp;
    return len;
}

size_t minim_utf8_encode(uint32_t c, unsigned char out[4]) {
    if (c < 0x80) {
        out[0] = (unsigned char)c;
        return 1;
    } else if (c < 0x800) {
        out[0] = (unsigned char)(0xC0 | (c >> 6));
        out[1] = (unsigned char)(0x80 | (c & 0x3F));
        return 2;
    } else if (c < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (c >> 12));
        out[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (c & 0x3F));
        return 3;
    } else {
        out[0] = (unsigned char)(0xF0 | (c >> 18));
        out[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (unsigned char)(0x80 | (c & 0x3F));
        return 4;
    }
}
