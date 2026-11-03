#include "test.h"

/*
 * [commands] — команды операционной системы.
 *
 * Проверяем чистую часть: подстановки %i, %o, %% в command_fmt2ptr() и
 * размеры буферов, в которые эти пути подставляются (иначе длинная команда
 * молча обрежется).
 */

START_TEST(er_commands_plain_text_is_untouched) {
    char* s = command_fmt2ptr("", "date > /tmp/out");
    ck_assert_str_eq(s, "date > /tmp/out");
    free(s);
}
END_TEST

START_TEST(er_commands_percent_i_is_input_file) {
    char expected[1024];
    snprintf(expected, sizeof expected, "bash < %s", curcommand_infile);

    char* s = command_fmt2ptr("выделенный текст", "bash < %i");
    ck_assert_str_eq(s, expected);
    free(s);

    // Входной файл содержит выделенное
    char got[256];
    ck_assert_str_eq(er_read(curcommand_infile, got, sizeof got), "выделенный текст");
}
END_TEST

START_TEST(er_commands_percent_o_is_output_file) {
    char expected[1024];
    snprintf(expected, sizeof expected, "date > %s", curcommand_outfile);

    char* s = command_fmt2ptr("", "date > %o");
    ck_assert_str_eq(s, expected);
    free(s);
}
END_TEST

START_TEST(er_commands_percent_percent_is_escaped) {
    // %% в конфиге — это экранированный %, а не подстановка
    char* s = command_fmt2ptr("", "printf 100%%");
    ck_assert_str_eq(s, "printf 100%");
    free(s);
}
END_TEST

START_TEST(er_commands_unknown_substitution_kept) {
    char* s = command_fmt2ptr("", "echo %x");
    ck_assert_str_eq(s, "echo %x");
    free(s);
}
END_TEST

START_TEST(er_commands_both_substitutions_in_a_row) {
    char expected[2048];
    snprintf(expected, sizeof expected, "trans -i %s -o %s", curcommand_infile, curcommand_outfile);

    char* s = command_fmt2ptr("", "trans -i %i -o %o");
    ck_assert_str_eq(s, expected);
    free(s);
}
END_TEST

START_TEST(er_commands_percent_at_end_is_kept) {
    char* s = command_fmt2ptr("", "echo 50%");
    ck_assert_str_eq(s, "echo 50%");
    free(s);
}
END_TEST

START_TEST(er_commands_long_command_is_not_truncated) {
    // Две пары %i %o при путях в /tmp дают ~100 символов: буфер в монолите
    // должен вмещать, иначе bash получит обрезанную команду
    char command[4096];
    size_t n = 0;
    for(int i = 0; i < 2; i++)
        n += snprintf(command + n, sizeof command - n, "echo %%i %%o ");

    char* s = command_fmt2ptr("", command);
    ck_assert_msg(strlen(s) > 90, "команда обрезана: %zu символов", strlen(s));
    ck_assert_ptr_nonnull(strstr(s, curcommand_infile));
    ck_assert_ptr_nonnull(strstr(s, curcommand_outfile));
    free(s);
}
END_TEST

START_TEST(er_commands_is_blank) {
    ck_assert_int_eq(is_blank(""), 1);
    ck_assert_int_eq(is_blank("   \t\n"), 1);
    ck_assert_int_eq(is_blank("x"), 0);
    ck_assert_int_eq(is_blank("  x  "), 0);
}
END_TEST

START_TEST(er_commands_parse_into_keyfn) {
    er_keyboard_config();
    er_load(
        "[commands]\n"
        "Alt+Control+d=date > %o\n"
        "Control+F1=bash < %i\n");

    ck_assert_int_eq(keyfn_size, 2);
    // В [commands] значение — часть строки после первого `=`,
    // а `%i` и `%o` в ней остаются как есть (подстановка — при запуске)
    // arg — сама команда, без подстановок (они делаются при запуске)
    int found = 0;
    for(int i = 0; i < keyfn_size; i++) {
        if(keyfn[i].action == CONFIG_ACTION_COMMAND && keyfn[i].arg && strcmp(keyfn[i].arg, "date > %o") == 0) found++;
        if(keyfn[i].action == CONFIG_ACTION_COMMAND && keyfn[i].arg && strcmp(keyfn[i].arg, "bash < %i") == 0) found++;
    }
    ck_assert_int_eq(found, 2);
}
END_TEST

static TCase* tcase_commands() {
    TCase* tc = tcase_create("подстановки");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_commands_plain_text_is_untouched);
    tcase_add_test(tc, er_commands_percent_i_is_input_file);
    tcase_add_test(tc, er_commands_percent_o_is_output_file);
    tcase_add_test(tc, er_commands_percent_percent_is_escaped);
    tcase_add_test(tc, er_commands_unknown_substitution_kept);
    tcase_add_test(tc, er_commands_both_substitutions_in_a_row);
    tcase_add_test(tc, er_commands_percent_at_end_is_kept);
    tcase_add_test(tc, er_commands_long_command_is_not_truncated);
    tcase_add_test(tc, er_commands_is_blank);
    tcase_add_test(tc, er_commands_parse_into_keyfn);
    return tc;
}

/*
 * Сценарий commands проверяется на фейковых портах: без fork, файлов и
 * настоящих таймеров. Так видно, что %i/%o, таймаут и запрет параллельного
 * запуска остались в use_cases, а не размазаны по монолиту.
 */

typedef struct {
    int prepare_calls;
    int start_calls;
    int poll_calls;
    int cancel_calls;
    char command[512];
    char input[512];
    int input_len;
    int capture_output;
    command_runner_status_t poll_status;
    char output[512];
    int output_len;
} er_fake_runner_t;

static char* er_fake_runner_prepare(command_runner_port_t* port, const char* command,
                                    const char* input, int input_len) {
    er_fake_runner_t* state = (er_fake_runner_t*) port->context;
    state->prepare_calls++;
    snprintf(state->command, sizeof state->command, "%s", command ? command : "");
    state->input_len = input_len;
    if(input && input_len > 0) {
        int n = input_len;
        if(n > (int) sizeof state->input - 1) n = (int) sizeof state->input - 1;
        memcpy(state->input, input, (size_t) n);
        state->input[n] = '\0';
    } else {
        state->input[0] = '\0';
    }
    return strdup(command ? command : "");
}

static int er_fake_runner_start(command_runner_port_t* port, char* command,
                                int capture_output) {
    er_fake_runner_t* state = (er_fake_runner_t*) port->context;
    state->start_calls++;
    state->capture_output = capture_output;
    free(command);
    return 1;
}

static command_runner_status_t er_fake_runner_poll(command_runner_port_t* port) {
    er_fake_runner_t* state = (er_fake_runner_t*) port->context;
    state->poll_calls++;
    return state->poll_status;
}

static void er_fake_runner_cancel(command_runner_port_t* port) {
    er_fake_runner_t* state = (er_fake_runner_t*) port->context;
    state->cancel_calls++;
}

static const char* er_fake_runner_output(command_runner_port_t* port, int* len) {
    er_fake_runner_t* state = (er_fake_runner_t*) port->context;
    if(len) *len = state->output_len;
    return state->output;
}

static void er_fake_runner_init(command_runner_port_t* port, er_fake_runner_t* state) {
    memset(state, 0, sizeof(*state));
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->prepare = er_fake_runner_prepare;
    port->start = er_fake_runner_start;
    port->poll = er_fake_runner_poll;
    port->cancel = er_fake_runner_cancel;
    port->output = er_fake_runner_output;
}

typedef struct {
    int after_calls;
    double last_interval;
    scheduler_callback_t callback;
    void* context;
} er_fake_scheduler_t;

static int er_fake_scheduler_after(scheduler_port_t* port, double seconds,
                                   scheduler_callback_t callback, void* context) {
    er_fake_scheduler_t* state = (er_fake_scheduler_t*) port->context;
    state->after_calls++;
    state->last_interval = seconds;
    state->callback = callback;
    state->context = context;
    return 0;
}

static void er_fake_scheduler_init(scheduler_port_t* port, er_fake_scheduler_t* state) {
    memset(state, 0, sizeof(*state));
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->after = er_fake_scheduler_after;
}

typedef struct {
    int get_calls;
    clipboard_get_callback_t callback;
    void* context;
} er_fake_clipboard_t;

static void er_fake_clipboard_get(clipboard_port_t* port, clipboard_selection_t selection,
                                  clipboard_format_t format,
                                  clipboard_get_callback_t callback, void* context) {
    (void) selection;
    (void) format;
    er_fake_clipboard_t* state = (er_fake_clipboard_t*) port->context;
    state->get_calls++;
    state->callback = callback;
    state->context = context;
}

static void er_fake_clipboard_set(clipboard_port_t* port, char* data, int len,
                                  clipboard_format_t format) {
    (void) port;
    (void) data;
    (void) len;
    (void) format;
}

static void er_fake_clipboard_clear(clipboard_port_t* port) {
    (void) port;
}

static void er_fake_clipboard_init(clipboard_port_t* port, er_fake_clipboard_t* state) {
    memset(state, 0, sizeof(*state));
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->get = er_fake_clipboard_get;
    port->set = er_fake_clipboard_set;
    port->clear = er_fake_clipboard_clear;
}

typedef struct {
    int request_calls;
    int insert_calls;
    char insert_text[512];
} er_fake_command_context_t;

static void er_fake_request_selection(void* context) {
    ((er_fake_command_context_t*) context)->request_calls++;
}

static void er_fake_insert_text(void* context, const char* text) {
    er_fake_command_context_t* state = (er_fake_command_context_t*) context;
    state->insert_calls++;
    snprintf(state->insert_text, sizeof state->insert_text, "%s", text);
}

typedef struct {
    command_runner_port_t runner;
    er_fake_runner_t runner_state;
    clipboard_port_t clipboard;
    er_fake_clipboard_t clipboard_state;
    scheduler_port_t scheduler;
    er_fake_scheduler_t scheduler_state;
    er_fake_command_context_t context;
    commands_use_case_t use_case;
} er_commands_fixture_t;

static void er_commands_fixture_init(er_commands_fixture_t* f, double timeout,
                                     double interval) {
    memset(f, 0, sizeof(*f));
    er_fake_runner_init(&f->runner, &f->runner_state);
    er_fake_clipboard_init(&f->clipboard, &f->clipboard_state);
    er_fake_scheduler_init(&f->scheduler, &f->scheduler_state);
    commands_use_case_init(&f->use_case, &f->runner, &f->clipboard,
                           &f->scheduler, er_fake_request_selection, &f->context,
                           er_fake_insert_text, &f->context, timeout, interval);
}

static void er_commands_fixture_tick(er_commands_fixture_t* f) {
    ck_assert_ptr_nonnull(f->scheduler_state.callback);
    f->scheduler_state.callback(f->scheduler_state.context);
}

START_TEST(er_commands_use_case_runs_without_input) {
    er_commands_fixture_t f;
    er_commands_fixture_init(&f, 1.0, 0.1);
    f.runner_state.poll_status = COMMAND_RUNNER_DONE;

    commands_use_case_run(&f.use_case, "date");

    ck_assert_int_eq(f.runner_state.prepare_calls, 1);
    ck_assert_int_eq(f.runner_state.start_calls, 1);
    ck_assert_int_eq(f.runner_state.capture_output, 0);
    ck_assert_str_eq(f.runner_state.command, "date");
    ck_assert_str_eq(f.runner_state.input, "");
    ck_assert_int_eq(f.scheduler_state.after_calls, 1);
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 1);

    er_commands_fixture_tick(&f);

    ck_assert_int_eq(f.runner_state.poll_calls, 1);
    ck_assert_int_eq(f.context.insert_calls, 0);
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 0);
}
END_TEST

START_TEST(er_commands_use_case_waits_for_clipboard_and_inserts_output) {
    er_commands_fixture_t f;
    er_commands_fixture_init(&f, 1.0, 0.1);
    f.runner_state.poll_status = COMMAND_RUNNER_DONE;
    snprintf(f.runner_state.output, sizeof f.runner_state.output, "результат");
    f.runner_state.output_len = (int) strlen(f.runner_state.output);

    commands_use_case_run(&f.use_case, "trans < %i > %o");

    ck_assert_int_eq(f.context.request_calls, 1);
    ck_assert_int_eq(f.clipboard_state.get_calls, 1);
    ck_assert_int_eq(f.runner_state.prepare_calls, 0);
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 1);

    f.clipboard_state.callback(f.clipboard_state.context, "выделено",
                               (int) strlen("выделено"), CLIPBOARD_FORMAT_UTF8);

    ck_assert_int_eq(f.runner_state.prepare_calls, 1);
    ck_assert_int_eq(f.runner_state.start_calls, 1);
    ck_assert_int_eq(f.runner_state.capture_output, 1);
    ck_assert_str_eq(f.runner_state.input, "выделено");
    ck_assert_int_eq(f.scheduler_state.after_calls, 1);

    er_commands_fixture_tick(&f);

    ck_assert_int_eq(f.context.insert_calls, 1);
    ck_assert_str_eq(f.context.insert_text, "результат");
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 0);
}
END_TEST

START_TEST(er_commands_use_case_times_out) {
    er_commands_fixture_t f;
    er_commands_fixture_init(&f, 0.3, 0.1);
    f.runner_state.poll_status = COMMAND_RUNNER_RUNNING;

    commands_use_case_run(&f.use_case, "sleep 10");
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 1);

    er_commands_fixture_tick(&f);
    er_commands_fixture_tick(&f);
    ck_assert_int_eq(f.context.insert_calls, 0);
    er_commands_fixture_tick(&f);

    ck_assert_int_eq(f.runner_state.cancel_calls, 1);
    ck_assert_int_eq(f.context.insert_calls, 1);
    ck_assert_str_eq(f.context.insert_text, "TIMEOUT 0,3s!");
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 0);
}
END_TEST

START_TEST(er_commands_use_case_rejects_blank_and_parallel) {
    er_commands_fixture_t f;
    er_commands_fixture_init(&f, 1.0, 0.1);
    f.runner_state.poll_status = COMMAND_RUNNER_DONE;

    commands_use_case_run(&f.use_case, "  \t\n");
    ck_assert_int_eq(f.runner_state.prepare_calls, 0);

    commands_use_case_run(&f.use_case, "echo one");
    ck_assert_int_eq(f.runner_state.prepare_calls, 1);

    commands_use_case_run(&f.use_case, "echo two");
    ck_assert_int_eq(f.runner_state.prepare_calls, 1);

    er_commands_fixture_tick(&f);
    ck_assert_int_eq(commands_use_case_busy(&f.use_case), 0);

    commands_use_case_run(&f.use_case, "echo three");
    ck_assert_int_eq(f.runner_state.prepare_calls, 2);
}
END_TEST

static TCase* tcase_commands_use_case() {
    TCase* tc = tcase_create("сценарий");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_commands_use_case_runs_without_input);
    tcase_add_test(tc, er_commands_use_case_waits_for_clipboard_and_inserts_output);
    tcase_add_test(tc, er_commands_use_case_times_out);
    tcase_add_test(tc, er_commands_use_case_rejects_blank_and_parallel);
    return tc;
}

Suite* suite_commands() {
    Suite* s = suite_create("[commands]");
    suite_add_tcase(s, tcase_commands());
    suite_add_tcase(s, tcase_commands_use_case());
    return s;
}
