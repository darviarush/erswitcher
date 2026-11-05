#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/domain/unikey/unikey.h"
#include "../../src/use-cases/config-reload/config-reload.h"

#define FUZZ_MAX_INPUT 65536

static int fuzz_resolve_key(const char* name, unikey_t* key, void* context) {
    (void) context;

    if(strcmp(name, "Pause") == 0 || strcmp(name, "F1") == 0 ||
       strcmp(name, "Break") == 0) {
        key->code = 70;
        return 1;
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static char text[FUZZ_MAX_INPUT + 1];

    if(size > FUZZ_MAX_INPUT) return 0;

    memcpy(text, data, size);
    text[size] = '\0';

    config_reload_parse_context_t context = {
        .resolve_key = fuzz_resolve_key,
        .resolver_context = NULL,
        .log = NULL,
        .log_context = NULL,
        .base_options = NULL,
    };
    config_t config;

    int result = config_reload_parse(text, "fuzz.conf", &context, &config);

    assert(result == 0 || result == 1);
    assert(config.bindings_size >= 0);
    assert(config.bindings_capacity >= config.bindings_size);
    assert(config.compose_size >= 0);
    assert(config.compose_capacity >= config.compose_size);

    config_free(&config);
    return 0;
}
