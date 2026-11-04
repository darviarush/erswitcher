/*
 * Юнит-тесты чистых доменных модулей
 *
 * Собираются отдельным бинарником (tests/unit/test-domain), потому что
 * монолит пока определяет одноимённые функции (utf8_strlen, from_space…)
 * После шага 2 эти тесты можно будет слить с основным набором
 */

#include <check.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../../src/domain/unikey/unikey.h"
#include "../../src/domain/text/text.h"
#include "../../src/domain/input-buffer/input-buffer.h"
#include "../../src/domain/case-invert/case-invert.h"
#include "../../src/domain/compose/compose.h"
#include "../../src/domain/command-template/command-template.h"
#include "../../src/domain/config/config.h"

//@category unikey

START_TEST(unikey_mods_match_x11_values) {
    // Значения сохранены совпадающими с X11, чтобы перенос не менял поведение
    ck_assert_int_eq(ERS_MOD_SHIFT, 0x01);
    ck_assert_int_eq(ERS_MOD_LOCK, 0x02);
    ck_assert_int_eq(ERS_MOD_CONTROL, 0x04);
    ck_assert_int_eq(ERS_MOD_ALT, 0x08);
    ck_assert_int_eq(ERS_MOD_NUM, 0x10);
    ck_assert_int_eq(ERS_MOD_META, 0x20);
    ck_assert_int_eq(ERS_MOD_SUPER, 0x40);
    ck_assert_int_eq(ERS_MOD_HYPER, 0x80);
}
END_TEST

//@category text

START_TEST(text_utf8_strlen_ascii) {
    ck_assert_int_eq(utf8_strlen(""), 0);
    ck_assert_int_eq(utf8_strlen("abc"), 3);
}
END_TEST

START_TEST(text_utf8_strlen_cyrillic) {
    ck_assert_int_eq(utf8_strlen("привет"), 6);
    ck_assert_int_eq(utf8_strlen("Ёж"), 2);
}
END_TEST

START_TEST(text_utf8_strlen_mixed_and_emoji) {
    ck_assert_int_eq(utf8_strlen("abcы"), 4);
    ck_assert_int_eq(utf8_strlen("😀"), 1);
    ck_assert_int_eq(utf8_strlen("a😀b"), 3);
}
END_TEST

START_TEST(text_utf8_decode_ascii) {
    wint_t out = 0;
    ck_assert_int_eq(utf8_decode("a", &out), 1);
    ck_assert_int_eq(out, L'a');

    ck_assert_int_eq(utf8_decode("", &out), 0);
}
END_TEST

START_TEST(text_utf8_decode_unicode) {
    wint_t out = 0;
    ck_assert_int_eq(utf8_decode("ы", &out), 2);
    ck_assert_int_eq(out, L'ы');

    ck_assert_int_eq(utf8_decode("😀", &out), 4);
    ck_assert_int_eq(out, 0x1F600);
}
END_TEST

START_TEST(text_utf8_decode_invalid) {
    wint_t out = 0;
    // Обрезанная последовательность
    ck_assert_int_eq(utf8_decode("\xD0", &out), -1);
    // Байт-продолжение без начала
    ck_assert_int_eq(utf8_decode("\x80", &out), -1);
    // Overlong-последовательность
    ck_assert_int_eq(utf8_decode("\xC0\x80", &out), -1);
}
END_TEST

//@category input_buffer

static unikey_t key_of(int code) {
    return (unikey_t){code : code, mods : 0, group : 0};
}

static void push_str(input_buffer_t* buffer, const wchar_t* s) {
    for(int i = 0; s[i]; i++) {
        input_buffer_push(buffer, key_of(i + 1), s[i]);
    }
}

START_TEST(input_buffer_init_and_reset) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);

    ck_assert_int_eq(buffer.pos, 0);
    ck_assert_int_eq(input_buffer_from_space(&buffer), 0);

    push_str(&buffer, L"abc");
    ck_assert_int_eq(buffer.pos, 3);

    input_buffer_reset(&buffer);
    ck_assert_int_eq(buffer.pos, 0);
    ck_assert_int_eq(input_buffer_from_space(&buffer), 0);
}
END_TEST

START_TEST(input_buffer_from_space_tail_word) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    push_str(&buffer, L"abc xyz");

    ck_assert_int_eq(input_buffer_from_space(&buffer), 4);
}
END_TEST

START_TEST(input_buffer_from_space_single_word) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    push_str(&buffer, L"abc");

    ck_assert_int_eq(input_buffer_from_space(&buffer), 0);
}
END_TEST

START_TEST(input_buffer_from_space_unicode_space) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    push_str(&buffer, L"привет мир");

    ck_assert_int_eq(input_buffer_from_space(&buffer), 7);
}
END_TEST

START_TEST(input_buffer_from_space_trailing_space) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);

    // Хвостовой пробел — часть последнего слова
    push_str(&buffer, L"abc ");
    ck_assert_int_eq(input_buffer_from_space(&buffer), 0);

    input_buffer_init(&buffer);
    push_str(&buffer, L"abc xyz ");
    ck_assert_int_eq(input_buffer_from_space(&buffer), 4);

    input_buffer_init(&buffer);
    push_str(&buffer, L"abc  xyz  ");
    ck_assert_int_eq(input_buffer_from_space(&buffer), 5);

    input_buffer_init(&buffer);
    push_str(&buffer, L"привет мир ");
    ck_assert_int_eq(input_buffer_from_space(&buffer), 7);
}
END_TEST

START_TEST(input_buffer_from_space_only_spaces) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    push_str(&buffer, L"   ");

    ck_assert_int_eq(input_buffer_from_space(&buffer), 0);
}
END_TEST

START_TEST(input_buffer_backspace_removes_last) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    push_str(&buffer, L"ab");

    input_buffer_backspace(&buffer);
    ck_assert_int_eq(buffer.pos, 1);
    ck_assert_int_eq(buffer.word[0].code, 1);

    input_buffer_backspace(&buffer);
    ck_assert_int_eq(buffer.pos, 0);

    // На пустом буфере Backspace ничего не делает
    input_buffer_backspace(&buffer);
    ck_assert_int_eq(buffer.pos, 0);
}
END_TEST

START_TEST(input_buffer_wraps_at_capacity) {
    input_buffer_t buffer;
    input_buffer_init(&buffer);

    for(int i = 0; i < INPUT_BUFFER_CAPACITY + 5; i++) {
        input_buffer_push(&buffer, key_of(i % 250 + 1), L'a');
    }

    // Как в монолите: при переполнении pos сбрасывается в 0
    ck_assert_int_eq(buffer.pos, 5);
    ck_assert_int_eq(buffer.word[0].code, INPUT_BUFFER_CAPACITY % 250 + 1);
}
END_TEST

//@category case_invert

START_TEST(case_invert_toggles_shift) {
    unikey_t key = {code : 30, mods : ERS_MOD_CONTROL, group : 0};

    key = unikey_invert_shift(key);
    ck_assert_int_eq(key.mods, ERS_MOD_CONTROL | ERS_MOD_SHIFT);

    key = unikey_invert_shift(key);
    ck_assert_int_eq(key.mods, ERS_MOD_CONTROL);
    ck_assert_int_eq(key.code, 30);
}
END_TEST

START_TEST(case_invert_preserves_other_mods) {
    unikey_t key = {code : 31, mods : ERS_MOD_SHIFT | ERS_MOD_LOCK | ERS_MOD_ALT, group : 2};

    key = unikey_invert_shift(key);
    ck_assert_int_eq(key.mods, ERS_MOD_LOCK | ERS_MOD_ALT);
    ck_assert_int_eq(key.group, 2);
}
END_TEST

//@category compose

static void compose_set(compose_t* entry, const wchar_t* word, const char* to, int cursor_back) {
    memset(entry, 0, sizeof(*entry));
    entry->pos = (int) wcslen(word);
    for(int i = 0; i < entry->pos; i++)
        entry->word[i] = word[i];
    entry->to = (char*) to;
    entry->cursor_back = cursor_back;
}

START_TEST(compose_finds_tail_match) {
    compose_t map[2];
    compose_set(&map[0], L"abc", "X", 0);
    compose_set(&map[1], L"bc", "Y", 0);

    wint_t symbols[] = {L'z', L'a', L'b', L'c'};
    ck_assert_int_eq(compose_find(map, 2, symbols, 4), 0);
}
END_TEST

START_TEST(compose_prefers_first_entry) {
    compose_t map[2];
    compose_set(&map[0], L"abc", "long", 0);
    compose_set(&map[1], L"bc", "short", 0);

    wint_t symbols[] = {L'a', L'b', L'c'};
    ck_assert_int_eq(compose_find(map, 2, symbols, 3), 0);
}
END_TEST

START_TEST(compose_no_match) {
    compose_t map[1];
    compose_set(&map[0], L"abc", "X", 0);

    wint_t symbols[] = {L'a', L'b'};
    ck_assert_int_eq(compose_find(map, 1, symbols, 2), -1);
}
END_TEST

START_TEST(compose_empty_buffer) {
    compose_t map[1];
    compose_set(&map[0], L"abc", "X", 0);

    ck_assert_int_eq(compose_find(map, 1, NULL, 0), -1);
}
END_TEST

START_TEST(compose_empty_map) {
    wint_t symbols[] = {L'a'};
    ck_assert_int_eq(compose_find(NULL, 0, symbols, 1), -1);
}
END_TEST

START_TEST(compose_match_not_at_tail) {
    compose_t map[1];
    compose_set(&map[0], L"abc", "X", 0);

    wint_t symbols[] = {L'a', L'b', L'c', L'z'};
    ck_assert_int_eq(compose_find(map, 1, symbols, 4), -1);
}
END_TEST

//@category command_template

START_TEST(command_template_substitutes_i_and_o) {
    char* s = command_template_expand("cmd %i %o", "/tmp/in", "/tmp/out", NULL);
    ck_assert_ptr_nonnull(s);
    ck_assert_str_eq(s, "cmd /tmp/in /tmp/out");
    free(s);
}
END_TEST

START_TEST(command_template_percent_escape) {
    char* s = command_template_expand("echo 100%% %i", "in", "out", NULL);
    ck_assert_ptr_nonnull(s);
    ck_assert_str_eq(s, "echo 100% in");
    free(s);
}
END_TEST

START_TEST(command_template_unknown_placeholder) {
    char* s = command_template_expand("echo %z %", "in", "out", NULL);
    ck_assert_ptr_nonnull(s);
    ck_assert_str_eq(s, "echo %z %");
    free(s);
}
END_TEST

START_TEST(command_template_empty_and_null) {
    char* s = command_template_expand("", "in", "out", NULL);
    ck_assert_ptr_nonnull(s);
    ck_assert_str_eq(s, "");
    free(s);

    ck_assert_ptr_null(command_template_expand(NULL, "in", "out", NULL));
}
END_TEST

//@category config

static int domain_key_resolver(const char* name, unikey_t* key, void* context) {
    (void) context;

    if(strcmp(name, "Pause") == 0) {
        key->code = 70;
        key->group = 0;
        return 1;
    }

    if(strcmp(name, "Break") == 0) {
        key->code = 71;
        key->group = 1;
        return 1;
    }

    if(strcmp(name, "F1") == 0) {
        key->code = 74;
        key->group = 2;
        return 1;
    }

    return 0;
}

static int near(double value, double expected) {
    double diff = value - expected;
    return diff > -1e-9 && diff < 1e-9;
}

START_TEST(config_sections_and_actions) {
    ck_assert_int_eq(config_section_from_string("[functions]"), CONFIG_SECTION_FUNCTIONS);
    ck_assert_int_eq(config_section_from_string("[options]"), CONFIG_SECTION_OPTIONS);
    ck_assert_int_eq(config_section_from_string("[compose]"), CONFIG_SECTION_COMPOSE);
    ck_assert_int_eq(config_section_from_string("[sendkeys]"), CONFIG_SECTION_SENDKEYS);
    ck_assert_int_eq(config_section_from_string("[snippets]"), CONFIG_SECTION_SNIPPETS);
    ck_assert_int_eq(config_section_from_string("[commands]"), CONFIG_SECTION_COMMANDS);
    ck_assert_int_eq(config_section_from_string("[nope]"), CONFIG_SECTION_UNKNOWN);
    ck_assert_int_eq(config_section_from_string(NULL), CONFIG_SECTION_UNKNOWN);

    ck_assert_int_eq(config_action_from_function("word translate"), CONFIG_ACTION_WORD_TRANSLATE);
    ck_assert_int_eq(config_action_from_function("text translate"), CONFIG_ACTION_TEXT_TRANSLATE);
    ck_assert_int_eq(config_action_from_function("selected translate"), CONFIG_ACTION_SELECTED_TRANSLATE);
    ck_assert_int_eq(config_action_from_function("word invertcase"), CONFIG_ACTION_WORD_INVERTCASE);
    ck_assert_int_eq(config_action_from_function("text invertcase"), CONFIG_ACTION_TEXT_INVERTCASE);
    ck_assert_int_eq(config_action_from_function("selected invertcase"), CONFIG_ACTION_SELECTED_INVERTCASE);
    ck_assert_int_eq(config_action_from_function("compose"), CONFIG_ACTION_COMPOSE);
    ck_assert_int_eq(config_action_from_function("make coffee"), CONFIG_ACTION_NONE);
    ck_assert_int_eq(config_action_from_function(NULL), CONFIG_ACTION_NONE);

    ck_assert_int_eq(config_action_for_section(CONFIG_SECTION_SENDKEYS), CONFIG_ACTION_SENDKEYS);
    ck_assert_int_eq(config_action_for_section(CONFIG_SECTION_SNIPPETS), CONFIG_ACTION_SNIPPET);
    ck_assert_int_eq(config_action_for_section(CONFIG_SECTION_COMMANDS), CONFIG_ACTION_COMMAND);
    ck_assert_int_eq(config_action_for_section(CONFIG_SECTION_FUNCTIONS), CONFIG_ACTION_NONE);
}
END_TEST

START_TEST(config_parse_seconds_locale_independent) {
    double value = 0;

    ck_assert_int_eq(config_parse_seconds("0,5", &value), 1);
    ck_assert(near(value, 0.5));

    ck_assert_int_eq(config_parse_seconds("1.25", &value), 1);
    ck_assert(near(value, 1.25));

    ck_assert_int_eq(config_parse_seconds("  2  ", &value), 1);
    ck_assert(near(value, 2.0));

    ck_assert_int_eq(config_parse_seconds("1e-2", &value), 1);
    ck_assert(near(value, 0.01));

    ck_assert_int_eq(config_parse_seconds("0", &value), 1);
    ck_assert(near(value, 0.0));

    ck_assert_int_eq(config_parse_seconds("-1", &value), 1);
    ck_assert(near(value, -1.0));

    ck_assert_int_eq(config_parse_seconds("abc", &value), 0);
    ck_assert_int_eq(config_parse_seconds("", &value), 0);
    ck_assert_int_eq(config_parse_seconds(NULL, &value), 0);
}
END_TEST

START_TEST(config_options_defaults_and_updates) {
    config_options_t options = config_options_default();
    ck_assert(near(options.delay, CONFIG_DEFAULT_DELAY));
    ck_assert(near(options.loop_delay, CONFIG_DEFAULT_LOOP_DELAY));
    ck_assert(near(options.command_timeout, CONFIG_DEFAULT_COMMAND_TIMEOUT));
    ck_assert(near(options.command_interval, CONFIG_DEFAULT_COMMAND_INTERVAL));

    ck_assert_int_eq(config_set_option(&options, "delay", "0,5"), CONFIG_OPTION_OK);
    ck_assert(near(options.delay, 0.5));

    ck_assert_int_eq(config_set_option(&options, "loop_delay", "0.02"), CONFIG_OPTION_OK);
    ck_assert(near(options.loop_delay, 0.02));

    ck_assert_int_eq(config_set_option(&options, "command_timeout", "3"), CONFIG_OPTION_OK);
    ck_assert(near(options.command_timeout, 3.0));

    ck_assert_int_eq(config_set_option(&options, "command_interval", "0,05"), CONFIG_OPTION_OK);
    ck_assert(near(options.command_interval, 0.05));

    ck_assert_int_eq(config_set_option(&options, "delay", "abc"), CONFIG_OPTION_INVALID);
    ck_assert(near(options.delay, CONFIG_DEFAULT_DELAY));

    ck_assert_int_eq(config_set_option(&options, "delay", "0"), CONFIG_OPTION_INVALID);
    ck_assert(near(options.delay, CONFIG_DEFAULT_DELAY));

    // Меньше микросекунды старый код тоже считал ошибкой
    ck_assert_int_eq(config_set_option(&options, "delay", "0.0000001"), CONFIG_OPTION_INVALID);
    ck_assert(near(options.delay, CONFIG_DEFAULT_DELAY));

    ck_assert_int_eq(config_set_option(&options, "command_timeout", "-2"), CONFIG_OPTION_INVALID);
    ck_assert(near(options.command_timeout, CONFIG_DEFAULT_COMMAND_TIMEOUT));

    double saved = options.loop_delay;
    ck_assert_int_eq(config_set_option(&options, "no_such_option", "1"), CONFIG_OPTION_UNKNOWN);
    ck_assert(near(options.loop_delay, saved));
}
END_TEST

START_TEST(config_key_expression_modifiers) {
    unikey_t key = {0};

    ck_assert_int_eq(config_parse_key_expression("Shift+Alt+Pause", domain_key_resolver, NULL, &key), CONFIG_KEY_OK);
    ck_assert_int_eq(key.code, 70);
    ck_assert_int_eq(key.mods, ERS_MOD_SHIFT | ERS_MOD_ALT);
    ck_assert_int_eq(key.group, 0);

    ck_assert_int_eq(config_parse_key_expression("Ctrl+Option+Win+F1", domain_key_resolver, NULL, &key), CONFIG_KEY_OK);
    ck_assert_int_eq(key.code, 74);
    ck_assert_int_eq(key.mods, ERS_MOD_CONTROL | ERS_MOD_ALT | ERS_MOD_SUPER);
    ck_assert_int_eq(key.group, 2);
}
END_TEST

START_TEST(config_key_expression_errors) {
    unikey_t key = {0};

    ck_assert_int_eq(config_parse_key_expression("Control", domain_key_resolver, NULL, &key), CONFIG_KEY_NO_KEY);
    ck_assert_int_eq(config_parse_key_expression("", domain_key_resolver, NULL, &key), CONFIG_KEY_NO_KEY);
    ck_assert_int_eq(config_parse_key_expression(NULL, domain_key_resolver, NULL, &key), CONFIG_KEY_NO_KEY);
    ck_assert_int_eq(config_parse_key_expression("Pause+F1", domain_key_resolver, NULL, &key), CONFIG_KEY_MULTIPLE_KEYS);
    ck_assert_int_eq(config_parse_key_expression("Pause+Shift", domain_key_resolver, NULL, &key), CONFIG_KEY_MULTIPLE_KEYS);
    ck_assert_int_eq(config_parse_key_expression("NoSuchKey", domain_key_resolver, NULL, &key), CONFIG_KEY_UNKNOWN_KEY);
}
END_TEST

START_TEST(config_unescape_value_and_cursor) {
    char value[] = "a\\nb";
    ck_assert_ptr_eq(config_unescape_value(value, NULL), value);
    ck_assert_str_eq(value, "a\nb");

    char tab[] = "a\\tb";
    config_unescape_value(tab, NULL);
    ck_assert_str_eq(tab, "a\tb");

    char slash[] = "a\\\\b";
    config_unescape_value(slash, NULL);
    ck_assert_str_eq(slash, "a\\b");

    char unknown[] = "a\\qb";
    config_unescape_value(unknown, NULL);
    ck_assert_str_eq(unknown, "a\\qb");

    char trailing[] = "a\\";
    config_unescape_value(trailing, NULL);
    ck_assert_str_eq(trailing, "a\\");

    char with_cursor[] = "ab\\^cd";
    char* cursor = NULL;
    config_unescape_value(with_cursor, &cursor);
    ck_assert_str_eq(with_cursor, "abcd");
    ck_assert_ptr_eq(cursor, with_cursor + 2);
    ck_assert_str_eq(cursor, "cd");
    ck_assert_int_eq(utf8_strlen(cursor), 2);
}
END_TEST

START_TEST(config_parse_compose_key_escapes) {
    wint_t out[CONFIG_COMPOSE_KEY_MAX];

    ck_assert_int_eq(config_parse_compose_key("abc", out, CONFIG_COMPOSE_KEY_MAX), 3);
    ck_assert_int_eq(out[0], L'a');
    ck_assert_int_eq(out[1], L'b');
    ck_assert_int_eq(out[2], L'c');

    ck_assert_int_eq(config_parse_compose_key("ы", out, CONFIG_COMPOSE_KEY_MAX), 1);
    ck_assert_int_eq(out[0], L'ы');

    ck_assert_int_eq(config_parse_compose_key("<\\=", out, CONFIG_COMPOSE_KEY_MAX), 2);
    ck_assert_int_eq(out[0], L'<');
    ck_assert_int_eq(out[1], L'=');

    ck_assert_int_eq(config_parse_compose_key("\\\\", out, CONFIG_COMPOSE_KEY_MAX), 1);
    ck_assert_int_eq(out[0], L'\\');

    ck_assert_int_eq(config_parse_compose_key("", out, CONFIG_COMPOSE_KEY_MAX), 0);
    ck_assert_int_eq(config_parse_compose_key("\\", out, CONFIG_COMPOSE_KEY_MAX), 0);
    ck_assert_int_eq(config_parse_compose_key("abcd", out, 3), -1);
}
END_TEST

Suite* suite_domain(void) {
    Suite* s = suite_create("domain");

    TCase* tc = tcase_create("unikey");
    tcase_add_test(tc, unikey_mods_match_x11_values);
    suite_add_tcase(s, tc);

    tc = tcase_create("text");
    tcase_add_test(tc, text_utf8_strlen_ascii);
    tcase_add_test(tc, text_utf8_strlen_cyrillic);
    tcase_add_test(tc, text_utf8_strlen_mixed_and_emoji);
    tcase_add_test(tc, text_utf8_decode_ascii);
    tcase_add_test(tc, text_utf8_decode_unicode);
    tcase_add_test(tc, text_utf8_decode_invalid);
    suite_add_tcase(s, tc);

    tc = tcase_create("input_buffer");
    tcase_add_test(tc, input_buffer_init_and_reset);
    tcase_add_test(tc, input_buffer_from_space_tail_word);
    tcase_add_test(tc, input_buffer_from_space_single_word);
    tcase_add_test(tc, input_buffer_from_space_unicode_space);
    tcase_add_test(tc, input_buffer_from_space_trailing_space);
    tcase_add_test(tc, input_buffer_from_space_only_spaces);
    tcase_add_test(tc, input_buffer_backspace_removes_last);
    tcase_add_test(tc, input_buffer_wraps_at_capacity);
    suite_add_tcase(s, tc);

    tc = tcase_create("case_invert");
    tcase_add_test(tc, case_invert_toggles_shift);
    tcase_add_test(tc, case_invert_preserves_other_mods);
    suite_add_tcase(s, tc);

    tc = tcase_create("compose");
    tcase_add_test(tc, compose_finds_tail_match);
    tcase_add_test(tc, compose_prefers_first_entry);
    tcase_add_test(tc, compose_no_match);
    tcase_add_test(tc, compose_empty_buffer);
    tcase_add_test(tc, compose_empty_map);
    tcase_add_test(tc, compose_match_not_at_tail);
    suite_add_tcase(s, tc);

    tc = tcase_create("command_template");
    tcase_add_test(tc, command_template_substitutes_i_and_o);
    tcase_add_test(tc, command_template_percent_escape);
    tcase_add_test(tc, command_template_unknown_placeholder);
    tcase_add_test(tc, command_template_empty_and_null);
    suite_add_tcase(s, tc);

    tc = tcase_create("config");
    tcase_add_test(tc, config_sections_and_actions);
    tcase_add_test(tc, config_parse_seconds_locale_independent);
    tcase_add_test(tc, config_options_defaults_and_updates);
    tcase_add_test(tc, config_key_expression_modifiers);
    tcase_add_test(tc, config_key_expression_errors);
    tcase_add_test(tc, config_unescape_value_and_cursor);
    tcase_add_test(tc, config_parse_compose_key_escapes);
    suite_add_tcase(s, tc);

    return s;
}

int main(void) {
    Suite* suite = suite_domain();
    SRunner* runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);

    int run = srunner_ntests_run(runner);
    int bad = srunner_ntests_failed(runner);
    printf("%s: пройдено %i, провалено %i\n", bad ? "ПРОВАЛ" : "ok", run - bad, bad);
    srunner_free(runner);

    return bad ? 1 : 0;
}
