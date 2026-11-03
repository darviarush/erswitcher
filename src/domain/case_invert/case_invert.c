#include "case_invert.h"

unikey_t unikey_invert_shift(unikey_t key) {
    key.mods ^= ERS_MOD_SHIFT;
    return key;
}
