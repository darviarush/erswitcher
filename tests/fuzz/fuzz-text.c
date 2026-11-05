#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include "../../src/domain/text/text.h"

#define FUZZ_MAX_INPUT 65536

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static char text[FUZZ_MAX_INPUT + 1];

    if(size > FUZZ_MAX_INPUT) return 0;

    memcpy(text, data, size);
    text[size] = '\0';

    size_t i = 0;
    while(i < size) {
        wint_t symbol;
        int len = utf8_decode(text + i, &symbol);
        assert(len >= -1 && len <= 4);

        if(len <= 0) break;
        i += (size_t) len;
    }

    assert(utf8_strlen(text) >= 0);
    return 0;
}
