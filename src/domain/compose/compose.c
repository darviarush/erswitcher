#include "compose.h"

int compose_find(const compose_t* map, int size, const wint_t* symbols, int symbols_pos) {
    for(int i = 0; i < size; i++) {
        int k, n;
        for(k = symbols_pos - 1, n = map[i].pos - 1; k >= 0 && n >= 0; k--, n--) {
            if(map[i].word[n] != symbols[k]) break;
        }

        if(n == -1) return i;
    }

    return -1;
}
