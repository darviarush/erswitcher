#include "text.h"

int utf8_strlen(const char* s) {
    int count = 0;
    while(*s)
        count += (*s++ & 0xC0) != 0x80;
    return count;
}

int utf8_decode(const char* s, wint_t* out) {
    const unsigned char* p = (const unsigned char*) s;
    if(!p[0]) return 0;

    unsigned int code;
    int len;

    if(p[0] < 0x80) {
        code = p[0];
        len = 1;
    } else if((p[0] & 0xE0) == 0xC0) {
        code = p[0] & 0x1F;
        len = 2;
    } else if((p[0] & 0xF0) == 0xE0) {
        code = p[0] & 0x0F;
        len = 3;
    } else if((p[0] & 0xF8) == 0xF0) {
        code = p[0] & 0x07;
        len = 4;
    } else
        return -1;

    for(int i = 1; i < len; i++) {
        if((p[i] & 0xC0) != 0x80) return -1;
        code = (code << 6) | (p[i] & 0x3F);
    }

    // Запрещаем overlong-последовательности и суррогаты
    if((len == 2 && code < 0x80) || (len == 3 && code < 0x800) || (len == 4 && code < 0x10000) || (code >= 0xD800 && code <= 0xDFFF) || code > 0x10FFFF) return -1;

    if(out) *out = (wint_t) code;
    return len;
}
