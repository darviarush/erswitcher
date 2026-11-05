#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/domain/command-template/command-template.h"

#define FUZZ_MAX_INPUT 4096

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static char command[FUZZ_MAX_INPUT + 1];

    if(size > FUZZ_MAX_INPUT) return 0;

    memcpy(command, data, size);
    command[size] = '\0';

    int invalid = 0;
    char* expanded = command_template_expand(command, "/tmp/in", "/tmp/out", &invalid);

    assert(invalid >= 0);
    if(expanded) {
        assert(expanded[strlen(expanded)] == '\0');
        free(expanded);
    }

    return 0;
}
