#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include "../../src/domain/input-buffer/input-buffer.h"

#define FUZZ_MAX_OPS 8192

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);

    size_t count = size < FUZZ_MAX_OPS ? size : FUZZ_MAX_OPS;
    for(size_t i = 0; i < count; i++) {
        uint8_t op = data[i];

        switch(op & 0x03) {
        case 0:
            input_buffer_reset(&buffer);
            break;
        case 1:
            input_buffer_backspace(&buffer);
            break;
        default: {
            wint_t symbol = (op & 0x04) ? L' ' : (wint_t) (L'a' + (op % 26));
            unikey_t key = {.code = op};
            input_buffer_push(&buffer, key, symbol);
            break;
        }
        }

        assert(buffer.pos >= 0 && buffer.pos <= INPUT_BUFFER_CAPACITY);
        int from = input_buffer_from_space(&buffer);
        assert(from >= 0 && from <= buffer.pos);
    }

    return 0;
}
