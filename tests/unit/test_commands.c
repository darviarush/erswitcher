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
    // Две пары %i %o при путях в /tmp — больше 400 символов. Буфер в монолите
    // должен вмещать, иначе bash получит обрезанную команду
    char command[4096];
    size_t n = 0;
    for(int i = 0; i < 4; i++) n += snprintf(command + n, sizeof command - n, "echo %%i ");

    char* s = command_fmt2ptr("", command);
    ck_assert_msg(strlen(s) > 400, "команда обрезана: %zu символов", strlen(s));
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
    er_load(
        "[commands]\n"
        "Alt+Control+d=date > %o\n"
        "Control+F1=bash < %i\n"
    );

    ck_assert_int_eq(keyfn_size, 2);
    // arg — сама команда, без подстановок (они делаются при запуске)
    int found = 0;
    for(int i = 0; i < keyfn_size; i++) {
        if(keyfn[i].fn == run_command && keyfn[i].arg && strcmp(keyfn[i].arg, "date > %o") == 0) found++;
        if(keyfn[i].fn == run_command && keyfn[i].arg && strcmp(keyfn[i].arg, "bash < %i") == 0) found++;
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

Suite* suite_commands() {
    Suite* s = suite_create("[commands]");
    suite_add_tcase(s, tcase_commands());
    return s;
}
