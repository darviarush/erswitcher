#ifndef __ERSWITCHER_PORTS_CONFIG_STORE__H__
#define __ERSWITCHER_PORTS_CONFIG_STORE__H__

/*
 * Порт файлового хранилища конфигурации
 *
 * read() возвращает malloc-буфер с содержимым файла или NULL, если файла
 * нет/не удалось прочитать. ensure_default() создаёт каталог и файл
 * конфигурации по умолчанию, если его ещё нет
 */

typedef struct config_store_port config_store_port_t;

struct config_store_port {
    void* context;

    const char* (*path)(config_store_port_t* self);
    char* (*read)(config_store_port_t* self);
    int (*ensure_default)(config_store_port_t* self);
};

#endif /* __ERSWITCHER_PORTS_CONFIG_STORE__H__ */
