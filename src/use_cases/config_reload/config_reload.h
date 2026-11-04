#ifndef __ERSWITCHER_USE_CASES_CONFIG_RELOAD__H__
#define __ERSWITCHER_USE_CASES_CONFIG_RELOAD__H__

#include "../../domain/config/config.h"
#include "../../ports/config_store.h"

/*
 * Загрузка и разбор конфигурации
 *
 * Файловый ввод-вывод выполняет ports/config_store. Здесь только построение
 * нового снимка config_t: привязки хранят действие-enum и аргумент, без
 * function pointer и sentinel-значений
 */

typedef void (*config_reload_log_t)(void* context, const char* message);

typedef struct {
    config_key_resolver_t resolve_key;
    void* resolver_context;

    // Необязательный приёмник предупреждений. Если NULL — молча
    config_reload_log_t log;
    void* log_context;

    // Значения [options], которые нужно сохранить, если секция их не задаёт
    // NULL — значения по умолчанию
    const config_options_t* base_options;
} config_reload_parse_context_t;

// Разбирает текст в новый снимок. out всегда инициализируется; при ошибке
// разбора возвращает 0, но частично разобранные данные остаются в out и
// должны быть освобождены через config_free()
int config_reload_parse(
    const char* text,
    const char* path,
    const config_reload_parse_context_t* context,
    config_t* out);

typedef struct {
    config_store_port_t* store;
    const char* path;
    config_reload_parse_context_t parse;
} config_reload_context_t;

// Читает файл через store (при отсутствии создаёт default) и разбирает его
// При ошибке возвращает 0; out освобождается вызывающим
int config_reload(config_t* out, const config_reload_context_t* context);

#endif /* __ERSWITCHER_USE_CASES_CONFIG_RELOAD__H__ */
