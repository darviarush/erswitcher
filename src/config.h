#ifndef __CONFIG__H__
#define __CONFIG__H__

/**
 * Настройки приложения
 */

struct {
    int key;
    char mods;
} keymap_t;


struct {
//	keymap_t translate;
//	keymap_t translate;
} config;


void config_load();
void config_save();

#endif
