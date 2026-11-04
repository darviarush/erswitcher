#ifndef __ERSWITCHER_DOMAIN_CONFIG__H__
#define __ERSWITCHER_DOMAIN_CONFIG__H__

#include <stddef.h>
#include <wchar.h>

#include "../compose/compose.h"
#include "../unikey/unikey.h"

/*
 * Модель конфигурации и чистый разбор значений
 *
 * Здесь нет ни файлов, ни X11, ни setlocale. Преобразование имён клавиш
 * в unikey_t делает вызывающий через config_key_resolver_t: в приложении
 * это X11-порт, в тестах — простая таблица
 */

#define CONFIG_COMPOSE_KEY_MAX COMPOSE_KEY_MAX

typedef enum {
    CONFIG_SECTION_UNKNOWN = 0,
    CONFIG_SECTION_FUNCTIONS,
    CONFIG_SECTION_OPTIONS,
    CONFIG_SECTION_COMPOSE,
    CONFIG_SECTION_SENDKEYS,
    CONFIG_SECTION_SNIPPETS,
    CONFIG_SECTION_COMMANDS,
} config_section_t;

typedef enum {
    CONFIG_ACTION_NONE = 0,
    CONFIG_ACTION_WORD_TRANSLATE,
    CONFIG_ACTION_TEXT_TRANSLATE,
    CONFIG_ACTION_SELECTED_TRANSLATE,
    CONFIG_ACTION_WORD_INVERTCASE,
    CONFIG_ACTION_TEXT_INVERTCASE,
    CONFIG_ACTION_SELECTED_INVERTCASE,
    CONFIG_ACTION_COMPOSE,
    CONFIG_ACTION_SENDKEYS,
    CONFIG_ACTION_SNIPPET,
    CONFIG_ACTION_COMMAND,
} config_action_t;

#define CONFIG_DEFAULT_DELAY 0.01
#define CONFIG_DEFAULT_LOOP_DELAY 0.01
#define CONFIG_DEFAULT_COMMAND_TIMEOUT 13.0
#define CONFIG_DEFAULT_COMMAND_INTERVAL 0.1

typedef struct {
    double delay; // секунды
    double loop_delay; // секунды
    double command_timeout; // секунды
    double command_interval; // секунды
} config_options_t;

typedef struct {
    unikey_t key;
    config_action_t action;
    char* arg;
} config_binding_t;

typedef struct {
    config_options_t options;

    config_binding_t* bindings;
    int bindings_size;
    int bindings_capacity;

    compose_t* compose;
    int compose_size;
    int compose_capacity;
} config_t;

// Инициализирует пустой снимок конфига со значениями [options] по умолчанию
void config_init(config_t* config);

// Освобождает привязки и compose, оставляя options как есть
void config_clear_entries(config_t* config);

// Освобождает всё, включая options (сам struct остаётся валидным только
// после повторного config_init())
void config_free(config_t* config);

// Добавляет копию привязки. arg == NULL допустим для действий без аргумента
// Возвращает 1 при успехе
int config_add_binding(config_t* config, unikey_t key, config_action_t action, const char* arg);

// Добавляет копию мнемоники (duplicate to, копия word/pos/cursor_back)
// Возвращает 1 при успехе
int config_add_compose(config_t* config, const compose_t* item);

// Сортирует [compose] по убыванию длины левой части — как старый qsort
void config_sort_compose(config_t* config);

// Имя клавиши -> unikey_t без модификаторов
// Возвращает 1, если имя распознано
typedef int (*config_key_resolver_t)(const char* name, unikey_t* key, void* context);

typedef enum {
    CONFIG_OPTION_OK = 0,
    CONFIG_OPTION_UNKNOWN,
    CONFIG_OPTION_INVALID,
} config_option_result_t;

typedef enum {
    CONFIG_KEY_OK = 0,
    CONFIG_KEY_NO_KEY,
    CONFIG_KEY_MULTIPLE_KEYS,
    CONFIG_KEY_UNKNOWN_KEY,
} config_key_result_t;

config_section_t config_section_from_string(const char* s);
config_action_t config_action_from_function(const char* value);
config_action_t config_action_for_section(config_section_t section);

config_options_t config_options_default(void);

// Разбирает число с десятичной точкой или запятой независимо от locale
// Возвращает 1 при успехе
int config_parse_seconds(const char* value, double* out);

// Устанавливает опцию. При некорректном значении возвращает INVALID
// и восстанавливает значение по умолчанию — как старый load_config()
config_option_result_t config_set_option(config_options_t* options, const char* name, const char* value);

// Разбирает выражение вида "Shift+Control+Pause"
// Резолвер получает только имя клавиши, без модификаторов
config_key_result_t config_parse_key_expression(
    const char* expression,
    config_key_resolver_t resolver,
    void* context,
    unikey_t* out);

// Раскрывает \n, \t, \\ и маркер курсора \^ прямо в value
// cursor (может быть NULL) указывает на символ сразу после маркера
// Возвращает value
char* config_unescape_value(char* value, char** cursor);

// Разбирает левую часть [compose] в Unicode-символы
// Обратная косая черта экранирует следующий символ
// Возвращает количество символов или -1, если их больше max
int config_parse_compose_key(const char* key, wint_t* out, int max);

#endif /* __ERSWITCHER_DOMAIN_CONFIG__H__ */
