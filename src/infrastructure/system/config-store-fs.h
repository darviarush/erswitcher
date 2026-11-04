#ifndef __ERSWITCHER_INFRASTRUCTURE_SYSTEM_CONFIG_STORE_FS__H__
#define __ERSWITCHER_INFRASTRUCTURE_SYSTEM_CONFIG_STORE_FS__H__

#include "../../ports/config-store.h"

/*
 * Файловая реализация config_store
 *
 * Путь относительный: app_run() уже перешёл в $HOME через goto_home(),
 * поэтому `.config/erswitcher.conf` сохраняет прежнее поведение
 */

typedef struct {
    const char* path;
} config_store_fs_t;

extern config_store_fs_t er_config_store;

void config_store_fs_port(config_store_port_t* port, config_store_fs_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_SYSTEM_CONFIG_STORE_FS__H__ */
