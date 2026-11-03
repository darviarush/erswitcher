#include "test.h"

/*
 * use_cases/config_reload: построение снимка конфигурации без X11 и файлов.
 *
 * Файловый ввод-вывод проверяется старыми тестами load_config() через
 * песочницу HOME; здесь — только модель, резолвер клавиш и предупреждения.
 */

typedef struct {
    int count;
    char last[512];
} er_config_log_t;

static void er_config_log_cb(void* context, const char* message) {
    er_config_log_t* log = (er_config_log_t*) context;
    log->count++;
    snprintf(log->last, sizeof(log->last), "%s", message);
}

static int er_config_resolve(const char* name, unikey_t* key, void* context) {
    (void) context;

    unsigned code = 0;
    if(strcmp(name, "Pause") == 0)
        code = 70;
    else if(strcmp(name, "F1") == 0)
        code = 74;
    else if(strcmp(name, "F2") == 0)
        code = 75;
    else if(strcmp(name, "F3") == 0)
        code = 76;
    else
        return 0;

    *key = (unikey_t){.code = code};
    return 1;
}

static const config_binding_t* er_find_binding(const config_t* config, config_action_t action) {
    for(int i = 0; i < config->bindings_size; i++) {
        if(config->bindings[i].action == action) return &config->bindings[i];
    }
    return NULL;
}

START_TEST(er_config_reload_builds_model) {
    config_options_t base = config_options_default();
    base.delay = 0.25;

    er_config_log_t log = {0};
    config_reload_parse_context_t context = {
        .resolve_key = er_config_resolve,
        .resolver_context = NULL,
        .log = er_config_log_cb,
        .log_context = &log,
        .base_options = &base,
    };

    config_t config;
    ck_assert_int_eq(config_reload_parse(
                         "[functions]\n"
                         "Pause=word translate\n"
                         "Control+Pause=text translate\n"
                         "[options]\n"
                         "loop_delay=0,02\n"
                         "[compose]\n"
                         "ab=аб\n"
                         "[sendkeys]\n"
                         "F1=hello\n"
                         "[snippets]\n"
                         "F2=текст\n"
                         "[commands]\n"
                         "F3=echo hi\n",
                         "test.conf", &context, &config),
                     1);

    // Секция [options] не задаёт delay — значение из base сохраняется.
    ck_assert_double_eq_tol(config.options.delay, 0.25, 1e-9);
    ck_assert_double_eq_tol(config.options.loop_delay, 0.02, 1e-9);

    ck_assert_int_eq(config.bindings_size, 5);
    ck_assert_int_eq(config.compose_size, 1);
    ck_assert_str_eq(config.compose[0].to, "аб");
    ck_assert_int_eq(config.compose[0].pos, 2);

    const config_binding_t* word = er_find_binding(&config, CONFIG_ACTION_WORD_TRANSLATE);
    ck_assert_ptr_nonnull(word);
    ck_assert_int_eq(word->key.code, 70);
    ck_assert_int_eq(word->key.mods, 0);
    ck_assert_ptr_null(word->arg);

    const config_binding_t* text = er_find_binding(&config, CONFIG_ACTION_TEXT_TRANSLATE);
    ck_assert_ptr_nonnull(text);
    ck_assert_int_eq(text->key.mods, ERS_MOD_CONTROL);

    const config_binding_t* sendkeys = er_find_binding(&config, CONFIG_ACTION_SENDKEYS);
    ck_assert_ptr_nonnull(sendkeys);
    ck_assert_str_eq(sendkeys->arg, "hello");

    const config_binding_t* snippet = er_find_binding(&config, CONFIG_ACTION_SNIPPET);
    ck_assert_ptr_nonnull(snippet);
    ck_assert_str_eq(snippet->arg, "текст");

    const config_binding_t* command = er_find_binding(&config, CONFIG_ACTION_COMMAND);
    ck_assert_ptr_nonnull(command);
    ck_assert_str_eq(command->arg, "echo hi");

    config_free(&config);
}
END_TEST

START_TEST(er_config_reload_unescapes_values_and_cursor) {
    er_config_log_t log = {0};
    config_reload_parse_context_t context = {
        .resolve_key = er_config_resolve,
        .log = er_config_log_cb,
        .log_context = &log,
    };

    config_t config;
    ck_assert_int_eq(config_reload_parse(
                         "[compose]\n"
                         "ab=x\\^y\n"
                         "[snippets]\n"
                         "F1=a\\n\\t\\\\b\n",
                         "test.conf", &context, &config),
                     1);

    ck_assert_int_eq(config.compose_size, 1);
    ck_assert_str_eq(config.compose[0].to, "xy");
    ck_assert_int_eq(config.compose[0].cursor_back, 1);

    ck_assert_int_eq(config.bindings_size, 1);
    ck_assert_str_eq(config.bindings[0].arg, "a\n\t\\b");

    config_free(&config);
}
END_TEST

START_TEST(er_config_reload_reports_bad_lines) {
    er_config_log_t log = {0};
    config_reload_parse_context_t context = {
        .resolve_key = er_config_resolve,
        .log = er_config_log_cb,
        .log_context = &log,
    };

    config_t config;
    ck_assert_int_eq(config_reload_parse(
                         "[no_such_section]\n"
                         "Pause=word translate\n"
                         "[functions]\n"
                         "Pause word translate\n"
                         "Pause=no such function\n"
                         "NoSuchKey=word translate\n"
                         "[compose]\n"
                         "123456789012345678901=too long\n",
                         "test.conf", &context, &config),
                     1);

    ck_assert_int_eq(config.bindings_size, 0);
    ck_assert_int_eq(config.compose_size, 0);
    ck_assert_int_ge(log.count, 4);

    config_free(&config);
}
END_TEST

START_TEST(er_config_reload_sorts_compose_by_length) {
    er_config_log_t log = {0};
    config_reload_parse_context_t context = {
        .resolve_key = er_config_resolve,
        .log = er_config_log_cb,
        .log_context = &log,
    };

    config_t config;
    ck_assert_int_eq(config_reload_parse(
                         "[compose]\n"
                         "a=1\n"
                         "abc=3\n"
                         "ab=2\n",
                         "test.conf", &context, &config),
                     1);

    ck_assert_int_eq(config.compose_size, 3);
    ck_assert_int_eq(config.compose[0].pos, 3);
    ck_assert_int_eq(config.compose[1].pos, 2);
    ck_assert_int_eq(config.compose[2].pos, 1);
    ck_assert_str_eq(config.compose[0].to, "3");

    config_free(&config);
}
END_TEST

START_TEST(er_config_reload_invalid_option_falls_back_to_default) {
    config_options_t base = config_options_default();
    base.delay = 0.25;
    base.command_timeout = 42.0;

    er_config_log_t log = {0};
    config_reload_parse_context_t context = {
        .resolve_key = er_config_resolve,
        .log = er_config_log_cb,
        .log_context = &log,
        .base_options = &base,
    };

    config_t config;
    ck_assert_int_eq(config_reload_parse(
                         "[options]\n"
                         "delay=abc\n"
                         "command_timeout=0\n",
                         "test.conf", &context, &config),
                     1);

    ck_assert_double_eq_tol(config.options.delay, CONFIG_DEFAULT_DELAY, 1e-9);
    ck_assert_double_eq_tol(config.options.command_timeout, CONFIG_DEFAULT_COMMAND_TIMEOUT, 1e-9);
    ck_assert_int_eq(log.count, 2);

    config_free(&config);
}
END_TEST

Suite* suite_config_reload() {
    Suite* s = suite_create("config reload");

    TCase* tc = tcase_create("model");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_config_reload_builds_model);
    tcase_add_test(tc, er_config_reload_unescapes_values_and_cursor);
    tcase_add_test(tc, er_config_reload_reports_bad_lines);
    tcase_add_test(tc, er_config_reload_sorts_compose_by_length);
    tcase_add_test(tc, er_config_reload_invalid_option_falls_back_to_default);
    suite_add_tcase(s, tc);

    return s;
}
