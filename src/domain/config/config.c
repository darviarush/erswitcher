#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "../text/text.h"

#define CONFIG_GROW 256

void config_init(config_t* config) {
    if(!config) return;
    memset(config, 0, sizeof(*config));
    config->options = config_options_default();
}

void config_clear_entries(config_t* config) {
    if(!config) return;

    for(int i = 0; i < config->bindings_size; i++)
        free(config->bindings[i].arg);
    free(config->bindings);
    config->bindings = NULL;
    config->bindings_size = 0;
    config->bindings_capacity = 0;

    for(int i = 0; i < config->compose_size; i++)
        free(config->compose[i].to);
    free(config->compose);
    config->compose = NULL;
    config->compose_size = 0;
    config->compose_capacity = 0;
}

void config_free(config_t* config) {
    if(!config) return;
    config_clear_entries(config);
    memset(&config->options, 0, sizeof(config->options));
}

int config_add_binding(config_t* config, unikey_t key, config_action_t action, const char* arg) {
    if(!config) return 0;

    if(config->bindings_size >= config->bindings_capacity) {
        int capacity = config->bindings_capacity + CONFIG_GROW;
        config_binding_t* bindings = realloc(config->bindings, capacity * sizeof(*bindings));
        if(!bindings) return 0;
        config->bindings = bindings;
        config->bindings_capacity = capacity;
    }

    char* copy = arg ? strdup(arg) : NULL;
    if(arg && !copy) return 0;

    config->bindings[config->bindings_size++] = (config_binding_t){
        .key = key,
        .action = action,
        .arg = copy,
    };
    return 1;
}

int config_add_compose(config_t* config, const compose_t* item) {
    if(!config || !item) return 0;

    if(config->compose_size >= config->compose_capacity) {
        int capacity = config->compose_capacity + CONFIG_GROW;
        compose_t* compose = realloc(config->compose, capacity * sizeof(*compose));
        if(!compose) return 0;
        config->compose = compose;
        config->compose_capacity = capacity;
    }

    compose_t copy = *item;
    copy.to = item->to ? strdup(item->to) : NULL;
    if(item->to && !copy.to) return 0;

    config->compose[config->compose_size++] = copy;
    return 1;
}

static int compose_cmp(const void* a, const void* b) {
    return ((const compose_t*) b)->pos - ((const compose_t*) a)->pos;
}

void config_sort_compose(config_t* config) {
    if(!config || config->compose_size <= 1) return;
    qsort(config->compose, config->compose_size, sizeof(*config->compose), compose_cmp);
}

config_section_t config_section_from_string(const char* s) {
    if(!s) return CONFIG_SECTION_UNKNOWN;

    if(strcmp(s, "[functions]") == 0) return CONFIG_SECTION_FUNCTIONS;
    if(strcmp(s, "[options]") == 0) return CONFIG_SECTION_OPTIONS;
    if(strcmp(s, "[compose]") == 0) return CONFIG_SECTION_COMPOSE;
    if(strcmp(s, "[sendkeys]") == 0) return CONFIG_SECTION_SENDKEYS;
    if(strcmp(s, "[snippets]") == 0) return CONFIG_SECTION_SNIPPETS;
    if(strcmp(s, "[commands]") == 0) return CONFIG_SECTION_COMMANDS;

    return CONFIG_SECTION_UNKNOWN;
}

config_action_t config_action_from_function(const char* value) {
    if(!value) return CONFIG_ACTION_NONE;

    if(strcmp(value, "word translate") == 0) return CONFIG_ACTION_WORD_TRANSLATE;
    if(strcmp(value, "text translate") == 0) return CONFIG_ACTION_TEXT_TRANSLATE;
    if(strcmp(value, "selected translate") == 0) return CONFIG_ACTION_SELECTED_TRANSLATE;

    if(strcmp(value, "word invertcase") == 0) return CONFIG_ACTION_WORD_INVERTCASE;
    if(strcmp(value, "text invertcase") == 0) return CONFIG_ACTION_TEXT_INVERTCASE;
    if(strcmp(value, "selected invertcase") == 0) return CONFIG_ACTION_SELECTED_INVERTCASE;

    if(strcmp(value, "compose") == 0) return CONFIG_ACTION_COMPOSE;

    return CONFIG_ACTION_NONE;
}

config_action_t config_action_for_section(config_section_t section) {
    switch(section) {
    case CONFIG_SECTION_SENDKEYS: return CONFIG_ACTION_SENDKEYS;
    case CONFIG_SECTION_SNIPPETS: return CONFIG_ACTION_SNIPPET;
    case CONFIG_SECTION_COMMANDS: return CONFIG_ACTION_COMMAND;
    default: return CONFIG_ACTION_NONE;
    }
}

config_options_t config_options_default(void) {
    return (config_options_t){
        .delay = CONFIG_DEFAULT_DELAY,
        .loop_delay = CONFIG_DEFAULT_LOOP_DELAY,
        .command_timeout = CONFIG_DEFAULT_COMMAND_TIMEOUT,
        .command_interval = CONFIG_DEFAULT_COMMAND_INTERVAL,
    };
}

// Свой разбор числа: atof() зависит от locale, а запятая в конфиге
// должна работать независимо от setlocale()
static int parse_double(const char* s, double* out) {
    const char* p = s;
    while(isspace((unsigned char) *p))
        p++;

    int sign = 1;
    if(*p == '+' || *p == '-') {
        if(*p == '-') sign = -1;
        p++;
    }

    double value = 0;
    int digits = 0;

    while(isdigit((unsigned char) *p)) {
        value = value * 10 + (*p - '0');
        digits++;
        p++;
    }

    if(*p == '.' || *p == ',') {
        p++;
        double scale = 0.1;
        while(isdigit((unsigned char) *p)) {
            value += (*p - '0') * scale;
            scale *= 0.1;
            digits++;
            p++;
        }
    }

    if(!digits) return 0;

    if(*p == 'e' || *p == 'E') {
        p++;

        int exponent_sign = 1;
        if(*p == '+' || *p == '-') {
            if(*p == '-') exponent_sign = -1;
            p++;
        }

        int exponent = 0;
        int exponent_digits = 0;
        while(isdigit((unsigned char) *p)) {
            // Не накапливаем заведомо слишком большой порядок: иначе
            // умножение на 10 переполняет int ещё до проверки предела
            if(exponent > 308) return 0;
            exponent = exponent * 10 + (*p - '0');
            exponent_digits++;
            p++;
        }

        if(exponent_digits) {
            if(exponent > 308) return 0;
            while(exponent-- > 0)
                value *= exponent_sign > 0 ? 10 : 0.1;
        }
    }

    *out = sign * value;
    return 1;
}

int config_parse_seconds(const char* value, double* out) {
    if(!value || !out) return 0;
    return parse_double(value, out);
}

static config_option_result_t set_seconds(double* field, const char* value, double fallback) {
    double parsed;
    if(!config_parse_seconds(value, &parsed) || parsed <= 0) {
        *field = fallback;
        return CONFIG_OPTION_INVALID;
    }

    *field = parsed;
    return CONFIG_OPTION_OK;
}

// delay/loop_delay в монолите хранятся как целые микросекунды:
// значение, которое после умножения на 1e6 даёт 0, считается ошибкой
static config_option_result_t set_microseconds(double* field, const char* value, double fallback) {
    double parsed;
    if(!config_parse_seconds(value, &parsed) || parsed <= 0 || parsed * 1000000.0 < 1.0) {
        *field = fallback;
        return CONFIG_OPTION_INVALID;
    }

    *field = parsed;
    return CONFIG_OPTION_OK;
}

config_option_result_t config_set_option(config_options_t* options, const char* name, const char* value) {
    if(!options || !name) return CONFIG_OPTION_UNKNOWN;

    if(strcmp(name, "delay") == 0) {
        return set_microseconds(&options->delay, value, CONFIG_DEFAULT_DELAY);
    }

    if(strcmp(name, "loop_delay") == 0) {
        return set_microseconds(&options->loop_delay, value, CONFIG_DEFAULT_LOOP_DELAY);
    }

    if(strcmp(name, "command_timeout") == 0) {
        return set_seconds(&options->command_timeout, value, CONFIG_DEFAULT_COMMAND_TIMEOUT);
    }

    if(strcmp(name, "command_interval") == 0) {
        return set_seconds(&options->command_interval, value, CONFIG_DEFAULT_COMMAND_INTERVAL);
    }

    return CONFIG_OPTION_UNKNOWN;
}

static int token_eq(const char* start, size_t len, const char* value) {
    return strlen(value) == len && strncmp(start, value, len) == 0;
}

static int modifier_from_token(const char* start, size_t len, unsigned* mods) {
    if(token_eq(start, len, "Shift"))
        *mods |= ERS_MOD_SHIFT;
    else if(token_eq(start, len, "Control") || token_eq(start, len, "Ctrl"))
        *mods |= ERS_MOD_CONTROL;
    else if(token_eq(start, len, "Alt") || token_eq(start, len, "Option"))
        *mods |= ERS_MOD_ALT;
    else if(token_eq(start, len, "Meta"))
        *mods |= ERS_MOD_META;
    else if(token_eq(start, len, "Super") || token_eq(start, len, "Win") || token_eq(start, len, "Command"))
        *mods |= ERS_MOD_SUPER;
    else if(token_eq(start, len, "Hyper"))
        *mods |= ERS_MOD_HYPER;
    else
        return 0;

    return 1;
}

config_key_result_t config_parse_key_expression(
    const char* expression,
    config_key_resolver_t resolver,
    void* context,
    unikey_t* out) {
    if(!expression || !out) return CONFIG_KEY_NO_KEY;

    unsigned mods = 0;
    int key_set = 0;
    unikey_t key = {0};

    const char* p = expression;
    while(*p) {
        while(*p == '+')
            p++;
        if(!*p) break;

        const char* start = p;
        while(*p && *p != '+')
            p++;
        size_t len = (size_t) (p - start);

        // Как в старом парсере: после клавиши не может быть уже ничего
        if(key_set) return CONFIG_KEY_MULTIPLE_KEYS;

        if(modifier_from_token(start, len, &mods)) continue;

        if(len >= 128) return CONFIG_KEY_UNKNOWN_KEY;

        char name[128];
        memcpy(name, start, len);
        name[len] = '\0';

        unikey_t parsed = {0};
        if(!resolver || !resolver(name, &parsed, context)) return CONFIG_KEY_UNKNOWN_KEY;

        key = parsed;
        key_set = 1;
    }

    if(!key_set) return CONFIG_KEY_NO_KEY;

    key.mods = mods;
    *out = key;
    return CONFIG_KEY_OK;
}

char* config_unescape_value(char* value, char** cursor) {
    if(cursor) *cursor = NULL;
    if(!value) return NULL;

    for(char* x = value; *x; x++) {
        if(*x != '\\') continue;

        if(x[1] == '^') {
            if(cursor) *cursor = x;
            memmove(x, x + 2, strlen(x + 2) + 1);
            continue;
        }

        char to = 0;
        switch(x[1]) {
        case '\\': to = '\\'; break;
        case 'n': to = '\n'; break;
        case 't': to = '\t'; break;
        }

        if(to) {
            *x = to;
            memmove(x + 1, x + 2, strlen(x + 2) + 1);
        }
    }

    return value;
}

int config_parse_compose_key(const char* key, wint_t* out, int max) {
    if(!key || !out || max <= 0) return 0;

    int i = 0;
    const char* p = key;

    while(*p) {
        wint_t symbol;
        int len = utf8_decode(p, &symbol);
        if(len <= 0) break;
        p += len;

        // Обратная косая черта экранирует следующий символ
        if(symbol == L'\\') {
            if(!*p) break;

            len = utf8_decode(p, &symbol);
            if(len <= 0) break;
            p += len;
        }

        if(i >= max) return -1;
        out[i++] = symbol;
    }

    return i;
}
