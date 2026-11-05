#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "config-reload.h"
#include "../../domain/text/text.h"

static void report_at(
    const config_reload_parse_context_t* context,
    const char* path,
    int lineno,
    const char* format,
    ...) {
    if(!context || !context->log) return;

    char text[768];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    char message[1024];
    snprintf(message, sizeof(message), "WARN: %s:%i: %s",
             path ? path : "config", lineno, text);
    context->log(context->log_context, message);
}

static void trim_trailing(char* s) {
    char* end = s + strlen(s);
    while(end > s && isspace((unsigned char) end[-1]))
        end--;
    *end = '\0';
}

static void report_option_error(
    const config_reload_parse_context_t* context,
    const char* path,
    int lineno,
    const char* name,
    const char* value,
    config_option_result_t result) {
    if(result == CONFIG_OPTION_UNKNOWN) {
        report_at(context, path, lineno,
                  "ошибка: неизвестная опция `%s`. Пропущено", name);
    } else if(result == CONFIG_OPTION_INVALID) {
        if(strcmp(name, "switch_language") == 0) {
            report_at(context, path, lineno,
                      "ошибка: пустое или слишком длинное значение switch_language");
        } else {
            report_at(context, path, lineno,
                      "ошибка: %s в секундах, а не `%s`. Пропущено", name, value);
        }
    }
}

static int add_binding_checked(
    config_t* out,
    unikey_t key,
    config_action_t action,
    const char* arg,
    const config_reload_parse_context_t* context,
    const char* path,
    int lineno) {
    if(config_add_binding(out, key, action, arg)) return 1;
    report_at(context, path, lineno, "ошибка: не хватает памяти. Пропущено");
    return 0;
}

int config_reload_parse(
    const char* text,
    const char* path,
    const config_reload_parse_context_t* context,
    config_t* out) {
    if(!out) return 0;

    config_init(out);
    if(context && context->base_options) out->options = *context->base_options;

    if(!text) return 0;

    char* copy = strdup(text);
    if(!copy) return 0;

    config_section_t section = CONFIG_SECTION_UNKNOWN;
    int lineno = 0;
    char* line = copy;

    while(line) {
        lineno++;

        char* next = strchr(line, '\n');
        if(next) {
            *next = '\0';
            next++;
        }

        // Как в старом парсере: комментарии и пустые строки ищем после
        // ведущих пробелов, но дальше работаем с исходной строкой
        char* s = line;
        while(isspace((unsigned char) *s))
            s++;
        if(*s == '#' || *s == '\0') {
            line = next;
            continue;
        }

        s = line;
        int is_section_line = (*s == '[');
        if(is_section_line || section == CONFIG_SECTION_FUNCTIONS) trim_trailing(s);

        if(is_section_line) {
            config_section_t parsed = config_section_from_string(s);
            if(parsed == CONFIG_SECTION_UNKNOWN) {
                report_at(context, path, lineno,
                          "не распознана секция %s. Пропущена", s);
                section = CONFIG_SECTION_UNKNOWN;
            } else {
                section = parsed;
            }
            line = next;
            continue;
        }

        if(section == CONFIG_SECTION_UNKNOWN) {
            line = next;
            continue;
        }

        // Отделяем ключ от значения. Обратная косая черта экранирует
        // следующий символ, поэтому `\=` не считается разделителем
        char* v = s;
        while(*v) {
            if(*v == '\\') {
                v++;
                if(*v == '\0') break;
                v++;
                continue;
            }
            if(*v == '=') break;
            v++;
        }

        if(*v != '=') {
            report_at(context, path, lineno,
                      "ошибка синтаксиса: нет `=` в `%s`. Пропущено", s);
            line = next;
            continue;
        }

        *v = '\0';
        v++;

        char* cursor = NULL;
        config_unescape_value(v, &cursor);

        if(section == CONFIG_SECTION_OPTIONS) {
            config_option_result_t result = config_set_option(&out->options, s, v);
            report_option_error(context, path, lineno, s, v, result);
            line = next;
            continue;
        }

        if(section == CONFIG_SECTION_COMPOSE) {
            wint_t word[COMPOSE_KEY_MAX];
            int pos = config_parse_compose_key(s, word, COMPOSE_KEY_MAX);
            if(pos < 0) {
                report_at(context, path, lineno,
                          "строка слишком длинная \"%s\" > %i. Пропущено",
                          s, COMPOSE_KEY_MAX);
                line = next;
                continue;
            }

            compose_t item = {0};
            item.pos = pos;
            item.cursor_back = cursor ? utf8_strlen(cursor) : 0;
            item.to = (char*) v;
            if(pos > 0) memcpy(item.word, word, (size_t) pos * sizeof(word[0]));

            if(!config_add_compose(out, &item)) {
                report_at(context, path, lineno,
                          "ошибка: не хватает памяти. Пропущено");
            }
            line = next;
            continue;
        }

        unikey_t key = {0};
        config_key_result_t key_result = config_parse_key_expression(
            s,
            context ? context->resolve_key : NULL,
            context ? context->resolver_context : NULL,
            &key);

        switch(key_result) {
        case CONFIG_KEY_OK:
            break;
        case CONFIG_KEY_NO_KEY:
            report_at(context, path, lineno,
                      "ошибка синтаксиса: нет клавиши-немодификатора (в выражении %s). Пропущено",
                      s);
            line = next;
            continue;
        case CONFIG_KEY_MULTIPLE_KEYS:
            report_at(context, path, lineno,
                      "ошибка синтаксиса: несколько клавишь-немодификаторов подряд (в выражении %s). Пропущено",
                      s);
            line = next;
            continue;
        case CONFIG_KEY_UNKNOWN_KEY:
            report_at(context, path, lineno,
                      "не распознан символ в выражении %s. Пропущено", s);
            line = next;
            continue;
        }

        config_action_t action;
        const char* arg = NULL;

        if(section == CONFIG_SECTION_FUNCTIONS) {
            action = config_action_from_function(v);
            if(action == CONFIG_ACTION_NONE) {
                report_at(context, path, lineno,
                          "нет функции %s. Пропущено", v);
                line = next;
                continue;
            }
        } else {
            action = config_action_for_section(section);
            if(action == CONFIG_ACTION_NONE) {
                line = next;
                continue;
            }
            arg = v;
        }

        add_binding_checked(out, key, action, arg, context, path, lineno);
        line = next;
    }

    free(copy);
    config_sort_compose(out);
    return 1;
}

int config_reload(config_t* out, const config_reload_context_t* context) {
    if(!out) return 0;

    if(!context || !context->store) {
        config_init(out);
        if(context && context->parse.base_options) out->options = *context->parse.base_options;
        return 0;
    }

    const char* path = context->path;
    if(!path) path = context->store->path(context->store);
    if(!path) path = "config";

    char* text = context->store->read(context->store);
    if(!text) {
        if(!context->store->ensure_default(context->store)) {
            report_at(&context->parse, path, 0,
                      "не могу создать конфиг. Пропущено");
            config_init(out);
            if(context->parse.base_options) out->options = *context->parse.base_options;
            return 0;
        }

        text = context->store->read(context->store);
        if(!text) {
            report_at(&context->parse, path, 0,
                      "не могу открыть только что созданный конфиг");
            config_init(out);
            if(context->parse.base_options) out->options = *context->parse.base_options;
            return 0;
        }
    }

    int result = config_reload_parse(text, path, &context->parse, out);
    free(text);
    return result;
}
