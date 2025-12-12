#ifndef __ERSWITCHER_DOMAIN_BUFFER__H__
#define __ERSWITCHER_DOMAIN_BUFFER__H__

#include "../unikey/unikey.h"

// размер буфера word
#define ERSWITCHER_DOMAIN_BUFFER    1024

typedef struct {
    int pos;				                    // позиция в буфере символов
    unikey_t word[ERSWITCHER_DOMAIN_BUFFER];	// буфер символов
    int keys_pressed;			                // нажато клавишь
} buffer_t;

#endif /* __ERSWITCHER_DOMAIN_BUFFER__H__ */
