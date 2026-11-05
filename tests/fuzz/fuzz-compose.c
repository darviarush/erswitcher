#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include "../../src/domain/compose/compose.h"
#include "../../src/domain/config/config.h"

#define FUZZ_MAX_MAP 4
#define FUZZ_KEY_BYTES 256

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    compose_t map[FUZZ_MAX_MAP] = {0};
    wint_t symbols[COMPOSE_KEY_MAX] = {0};
    char key[FUZZ_KEY_BYTES + 1];

    size_t at = 0;
    int map_size = size > 0 ? data[at++] % FUZZ_MAX_MAP : 0;

    for(int i = 0; i < map_size; i++) {
        int pos = at < size ? data[at++] % (COMPOSE_KEY_MAX + 1) : 0;
        map[i].pos = pos;

        for(int j = 0; j < pos && at < size; j++)
            map[i].word[j] = (wint_t) data[at++];
    }

    int symbols_pos = at < size ? data[at++] % (COMPOSE_KEY_MAX + 1) : 0;
    for(int i = 0; i < symbols_pos && at < size; i++)
        symbols[i] = (wint_t) data[at++];

    int found = compose_find(map, map_size, symbols, symbols_pos);
    assert(found == -1 || (found >= 0 && found < map_size));

    size_t key_size = size < FUZZ_KEY_BYTES ? size : FUZZ_KEY_BYTES;
    memcpy(key, data, key_size);
    key[key_size] = '\0';

    wint_t out[COMPOSE_KEY_MAX];
    int parsed = config_parse_compose_key(key, out, COMPOSE_KEY_MAX);
    assert(parsed >= -1 && parsed <= COMPOSE_KEY_MAX);

    return 0;
}
